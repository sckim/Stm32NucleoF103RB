/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/**
 * 06_DMA_Priority_c  —  [계층: HAL]  DMA 채널 우선순위와 버스 중재: 두 채널이 동시에 전송할 때 누가 먼저 끝나는가
 *
 * 실험: DMA1 채널1 과 채널2 가 각각 4KB(1024워드) 메모리->메모리 복사를 "동시에" 시작한다.
 *       채널의 우선순위 설정에 따라 어느 채널이 먼저 끝나는지, 끝나는 시간이 어떻게 달라지는지 DWT 사이클로 측정한다.
 *
 * 중재 규칙 (RM0008 13.3.2 DMA 처리, "Arbiter")
 *   1) 소프트웨어 우선순위 (CCR.PL[1:0]): Very high > High > Medium > Low
 *   2) 소프트웨어 우선순위가 같으면 채널 번호가 낮은 쪽이 우선 (채널1 > 채널2 > ... > 채널7)
 *   중재는 "전송 1개(데이터 1워드) 단위"로 이루어진다. 높은 우선순위 채널이 요청을 계속 내는 동안에는 낮은 채널이 밀린다.
 *   메모리->메모리 모드는 요청이 항상 활성이라 우선순위 차이가 가장 극적으로 드러난다.
 *
 * B1(PC13)을 누를 때마다 설정이 바뀐다 (각 설정에서 시험을 3회 반복해 UART(115200)로 출력)
 *   케이스 A: 채널1 = Low,       채널2 = Very high   -> 채널2 가 먼저 끝나고, 채널1 은 그 뒤에 이어서 진행
 *   케이스 B: 채널1 = Very high, 채널2 = Low         -> 반대
 *   케이스 C: 둘 다 Medium                          -> 우선순위 같음: 낮은 번호(채널1)가 먼저
 *   출력: 각 채널의 완료 시각(시작 후 사이클 수)과 먼저 끝난 채널
 *
 * 인터럽트: 각 채널의 TC 인터럽트가 완료 시각(DWT->CYCCNT)만 기록한다.
 *   DMA1_Channel1/2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler() -> done1_cb / done2_cb (이 파일)
 *   * 인터럽트 진입 지연이 측정에 약간 섞이지만 채널 간 순서와 큰 차이를 보는 데는 충분하다.
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WORDS 1024

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DMA_HandleTypeDef hdma_memtomem_dma1_channel1;
DMA_HandleTypeDef hdma_memtomem_dma1_channel2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint32_t src[WORDS];
static uint32_t dst1[WORDS], dst2[WORDS];
static volatile uint32_t t_start, t_done1, t_done2;
static volatile uint8_t done1, done2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void dma_setup(uint32_t pl1, uint32_t pl2);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* HAL 콜백이 아니라 HAL_DMA_RegisterCallback 으로 등록하는 DMA 전송 완료 콜백 (ISR 문맥에서 호출됨) */
static void done1_cb(DMA_HandleTypeDef *h) { (void)h; t_done1 = DWT->CYCCNT - t_start; done1 = 1; }
static void done2_cb(DMA_HandleTypeDef *h) { (void)h; t_done2 = DWT->CYCCNT - t_start; done2 = 1; }
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  for (uint32_t i = 0; i < WORDS; i++) src[i] = 0xC0DE0000u + i;
  /* DMA1 클럭은 MX_DMA_Init 에서 이미 켜졌다 */

  static const struct { uint32_t p1, p2; const char *name; } cases[3] = {
      { DMA_PRIORITY_LOW,       DMA_PRIORITY_VERY_HIGH, "A: ch1=Low,       ch2=Very high" },
      { DMA_PRIORITY_VERY_HIGH, DMA_PRIORITY_LOW,       "B: ch1=Very high, ch2=Low      " },
      { DMA_PRIORITY_MEDIUM,    DMA_PRIORITY_MEDIUM,    "C: ch1=Medium,    ch2=Medium   " },
  };

  char msg[128];
  int c = 0;
  dma_setup(cases[c].p1, cases[c].p2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    for (int rep = 0; rep < 3; rep++)
    {
        memset(dst1, 0, sizeof dst1); memset(dst2, 0, sizeof dst2);
        done1 = done2 = 0;

        /* 두 채널을 가능한 한 동시에 시작 (인터럽트 금지 상태에서 연달아 시작) */
        __disable_irq();
        HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t)src, (uint32_t)dst1, WORDS);
        HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel2, (uint32_t)src, (uint32_t)dst2, WORDS);
        t_start = DWT->CYCCNT;
        __enable_irq();

        uint32_t t0 = HAL_GetTick();
        while ((!done1 || !done2) && (HAL_GetTick() - t0) < 100) {}

        int ok = (memcmp(src, dst1, sizeof src) == 0) && (memcmp(src, dst2, sizeof src) == 0);
        int n = snprintf(msg, sizeof msg, "[%s] ch1 done @ %6lu cyc | ch2 done @ %6lu cyc | first: ch%d | copy %s\r\n",
                         cases[c].name, (unsigned long)t_done1, (unsigned long)t_done2,
                         (t_done1 <= t_done2) ? 1 : 2, ok ? "OK" : "FAIL");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
        HAL_Delay(300);
    }
    HAL_UART_Transmit(&huart2, (uint8_t *)"--- press B1 for next case ---\r\n", 32, 100);
    while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) {}   /* B1 대기 */
    HAL_Delay(30);
    while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
    HAL_Delay(30);
    c = (c + 1) % 3;
    dma_setup(cases[c].p1, cases[c].p2);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  * Configure DMA for memory to memory transfers
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* Configure DMA request hdma_memtomem_dma1_channel1 on DMA1_Channel1 */
  hdma_memtomem_dma1_channel1.Instance = DMA1_Channel1;
  hdma_memtomem_dma1_channel1.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_channel1.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_channel1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_memtomem_dma1_channel1.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_memtomem_dma1_channel1.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_channel1.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_channel1) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_channel2 on DMA1_Channel2 */
  hdma_memtomem_dma1_channel2.Instance = DMA1_Channel2;
  hdma_memtomem_dma1_channel2.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_channel2.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_channel2.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_channel2.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_memtomem_dma1_channel2.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_memtomem_dma1_channel2.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_channel2.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_channel2) != HAL_OK)
  {
    Error_Handler( );
  }

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* 두 채널을 메모리->메모리로 다시 설정 (우선순위만 바꿔서) */
static void dma_setup(uint32_t pl1, uint32_t pl2)
{
    DMA_HandleTypeDef *h[2] = { &hdma_memtomem_dma1_channel1, &hdma_memtomem_dma1_channel2 };
    DMA_Channel_TypeDef *ch[2] = { DMA1_Channel1, DMA1_Channel2 };
    uint32_t pl[2] = { pl1, pl2 };
    for (int i = 0; i < 2; i++)
    {
        h[i]->Instance = ch[i];
        HAL_DMA_DeInit(h[i]);
        h[i]->Init.Direction = DMA_MEMORY_TO_MEMORY;             /* CCR.MEM2MEM = 1 */
        h[i]->Init.PeriphInc = DMA_PINC_ENABLE;
        h[i]->Init.MemInc = DMA_MINC_ENABLE;
        h[i]->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        h[i]->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        h[i]->Init.Mode = DMA_NORMAL;
        h[i]->Init.Priority = pl[i];                             /* CCR.PL[1:0] */
        if (HAL_DMA_Init(h[i]) != HAL_OK) Error_Handler();
    }
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel1, HAL_DMA_XFER_CPLT_CB_ID, done1_cb);
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel2, HAL_DMA_XFER_CPLT_CB_ID, done2_cb);
    /* NVIC(DMA1_Channel1/2_IRQn, 우선순위 1) 는 MX_DMA_Init 에서 이미 설정/활성화됨 */
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

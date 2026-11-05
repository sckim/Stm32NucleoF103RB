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
 * 01_DMA_MemToMem_c  —  [계층: HAL]  DMA1 채널1 메모리->메모리 복사
 *
 * 동작
 *   - 256워드(1KB) 배열을 (1) CPU 반복문, (2) DMA 로 각각 복사하고 DWT 사이클 카운터로 시간을 비교한다.
 *   - 부팅 시 1회, 이후 B1(PC13) 버튼을 누를 때마다 다시 측정하여 USART2(115200)로 출력.
 *
 * DMA 설정 (RM0008 13장)
 *   DMA1_Channel1->CCR: MEM2MEM=1, DIR=1(주변장치 주소 = 소스), PINC=1, MINC=1, PSIZE=MSIZE=32비트
 *   DMA1_Channel1->CPAR = 소스 주소, CMAR = 목적지 주소, CNDTR = 전송 개수
 *   DMA 는 CPU 를 쓰지 않고 버스 마스터로 복사 -> 완료되면 ISR.TCIF1 -> 인터럽트
 *
 * ISR vs 콜백
 *   DMA1_Channel1_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> dma_done_cb() (이 파일, HAL_DMA_RegisterCallback 으로 등록한 콜백)
 *
 * 참고: 데이터가 SRAM 안에서 왕복하므로 버스 경합 때문에 DMA 가 CPU 루프보다 극적으로
 *       빠르지는 않을 수 있다. 이 예제의 핵심은 "전송 중 CPU 가 자유로움"을 확인하는 것이다.
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
#define N_WORDS 256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DMA_HandleTypeDef hdma_memtomem_dma1_channel1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint32_t src[N_WORDS];
static uint32_t dst_cpu[N_WORDS];
static uint32_t dst_dma[N_WORDS];
static volatile uint8_t dma_done;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void run_test(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ------------------------------------------------------------------------- */
/* DMA 콜백 (HAL_DMA_IRQHandler(ISR, stm32f1xx_it.c) 가 호출하는 등록형 콜백 */
/* ------------------------------------------------------------------------- */
static void dma_done_cb(DMA_HandleTypeDef *hdma) { (void)hdma; dma_done = 1; }

static void run_test(void)
{
    char msg[96];
    uint32_t t0, cpu_cycles, dma_cycles, free_loops = 0;

    memset(dst_cpu, 0, sizeof dst_cpu);
    memset(dst_dma, 0, sizeof dst_dma);

    /* (1) CPU 복사. volatile 로 컴파일러가 memcpy 로 바꾸거나 제거하지 못하게 한다 */
    volatile uint32_t *d = dst_cpu;
    t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < N_WORDS; i++) d[i] = src[i];
    cpu_cycles = DWT->CYCCNT - t0;

    /* (2) DMA 복사: 시작 후 완료될 때까지 CPU 가 다른 일을 한 횟수(free_loops)도 센다 */
    dma_done = 0;
    t0 = DWT->CYCCNT;
    if (HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t)src, (uint32_t)dst_dma, N_WORDS) != HAL_OK) Error_Handler();
    while (!dma_done) free_loops++;
    dma_cycles = DWT->CYCCNT - t0;

    int ok = (memcmp(src, dst_dma, sizeof src) == 0);
    int n = snprintf(msg, sizeof msg, "CPU=%lu cyc  DMA=%lu cyc  (CPU free loops=%lu)  verify=%s\r\n",
                     (unsigned long)cpu_cycles, (unsigned long)dma_cycles,
                     (unsigned long)free_loops, ok ? "OK" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
}

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
  /* DWT 사이클 카운터: CoreDebug->DEMCR.TRCENA=1, DWT->CTRL.CYCCNTENA=1 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* MX_DMA_Init() 가 설정한 DMA1_Channel1 (메모리->메모리):
   *   CCR.MEM2MEM=1 / CCR.PINC=1 (소스 주소 증가) / CCR.MINC=1 (목적지 주소 증가) / CCR.PSIZE=CCR.MSIZE=32비트 / CCR.CIRC=0
   *   NVIC->ISER[0] bit11 (DMA1_Channel1_IRQn) 도 MX_DMA_Init 에서 활성화 */
  /* 전송 완료 콜백 등록 (HAL_DMA_IRQHandler 가 ISR.TCIF1 을 확인한 뒤 호출). 일반 HAL 콜백과 달리 함수 포인터 등록 방식 */
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel1, HAL_DMA_XFER_CPLT_CB_ID, dma_done_cb);

  for (uint32_t i = 0; i < N_WORDS; i++) src[i] = 0xA5000000u | i;

  run_test();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* GPIOC->IDR bit13 (B1_Pin) == 0 : 눌림 */
    {
      HAL_Delay(30);                                            /* 간이 디바운스 */
      run_test();
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
      HAL_Delay(30);
    }
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

/**
  * Enable DMA controller clock
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
  hdma_memtomem_dma1_channel1.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_channel1) != HAL_OK)
  {
    Error_Handler( );
  }

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

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

/* USER CODE BEGIN 4 */
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

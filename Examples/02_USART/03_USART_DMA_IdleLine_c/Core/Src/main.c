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
 * 03_USART_DMA_IdleLine_c  —  [계층: HAL]  USART2 DMA 수신 + IDLE 라인 감지 (가변 길이 패킷)
 *
 * 동작
 *   - PC 터미널(115200 8N1)에서 임의 길이의 문자열을 한 번에 보내면(한 줄 전송 등),
 *     DMA 가 CPU 개입 없이 수신 버퍼에 채우고, "라인이 조용해지는 순간(IDLE)" 한 번의 이벤트로
 *     패킷 길이와 내용을 알려 준다.  -> "[RX n bytes] ..." 로 되돌려 출력.
 *   - 바이트마다 인터럽트가 걸리는 02_USART_Interrupt_RX 와 비교: 인터럽트 횟수가 패킷당 1회로 줄어든다.
 *
 * 하드웨어 대응 (RM0008)
 *   USART2_RX  -> DMA1 채널6 (DMA1_Channel6->CPAR = &USART2->DR, CMAR = 수신 버퍼)
 *   USART2->CR3.DMAR = 1   : 수신 DMA 요청 허용
 *   USART2->CR1.IDLEIE = 1 : IDLE 인터럽트 허용 (SR.IDLE : 마지막 수신 후 1프레임 시간 동안 무신호)
 *   DMA CCR.CIRC = 1       : 원형 모드 (버퍼 끝에 닿으면 처음으로 되돌아감)
 *
 * ISR vs 콜백
 *   USART2_IRQHandler / DMA1_Channel6_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler / HAL_DMA_IRQHandler
 *     -> HAL_UARTEx_RxEventCallback() (이 파일) : IDLE 또는 DMA 전송 완료(TC) 시 호출, Size = 버퍼 내 현재 위치
 *   콜백은 ISR 문맥이므로 "패킷 복사 + 플래그"만 하고 출력은 main 에서 한다.
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
#define DMA_BUF_SIZE 128     /* DMA 원형 수신 버퍼 */
#define PKT_MAX      128

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
static uint8_t dma_buf[DMA_BUF_SIZE];
static uint16_t old_pos;                 /* 마지막으로 처리한 dma_buf 위치 */

static uint8_t pkt[PKT_MAX];             /* 콜백 -> main 전달용 패킷 */
static volatile uint16_t pkt_len;
static volatile uint8_t pkt_ready;
static volatile uint32_t pkt_dropped;    /* main 이 처리 못 해서 버려진 패킷 수 */
static volatile uint32_t event_count;    /* RxEvent 콜백(=인터럽트 이벤트) 횟수 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char head[40];
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
  const char *banner = "\r\n[USART2 DMA + IDLE] 문자열을 보내 보세요\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)banner, (uint16_t)strlen(banner), 100);

  /* DMA 수신 + IDLE 이벤트 시작. 반 버퍼(HT) 이벤트는 끄고 IDLE/TC 만 받는다 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, dma_buf, DMA_BUF_SIZE) != HAL_OK) Error_Handler();
  __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);          /* DMA1_Channel6->CCR.HTIE = 0 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (pkt_ready)
    {
      uint16_t len = pkt_len;
      int n = snprintf(head, sizeof head, "[RX %u bytes, events=%lu, drop=%lu] ",
                       (unsigned)len, (unsigned long)event_count, (unsigned long)pkt_dropped);
      HAL_UART_Transmit(&huart2, (uint8_t *)head, (uint16_t)n, 100);
      HAL_UART_Transmit(&huart2, pkt, len, 200);
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);
      pkt_ready = 0;
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
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

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
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): IDLE 감지 또는 DMA 버퍼 끝 도달 시 HAL 이 호출                 */
/* ------------------------------------------------------------------------- */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART2) return;
  event_count++;

  /* Size = dma_buf 안에서 "다음에 쓸 위치". 원형 버퍼이므로 old_pos -> Size 구간이 새 데이터 */
  uint16_t len = 0;
  if (Size != old_pos)
  {
    if (pkt_ready)
    {
      pkt_dropped++;                                   /* main 이 아직 이전 패킷을 안 가져감 */
    }
    else
    {
      if (Size > old_pos)
      {
        len = Size - old_pos;
        memcpy(pkt, &dma_buf[old_pos], len);
      }
      else                                             /* 버퍼 끝을 넘어 래핑된 경우 */
      {
        uint16_t first = DMA_BUF_SIZE - old_pos;
        memcpy(pkt, &dma_buf[old_pos], first);
        memcpy(pkt + first, dma_buf, Size);
        len = first + Size;
      }
      pkt_len = len;
      pkt_ready = 1;
    }
  }
  old_pos = (Size >= DMA_BUF_SIZE) ? 0 : Size;
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

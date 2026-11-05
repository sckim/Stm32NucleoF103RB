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
 * 02_DMA_Circular_GPIO_c  —  [계층: HAL]  원형(circular) DMA + 타이머 요청으로 GPIO 포트에 패턴을 자동 출력 (핑퐁 버퍼)
 *
 * CPU 없이 8비트 패턴을 정확한 주기로 GPIOC 에 내보낸다. 로직 분석기, LED 8개(저항 포함), 병렬 DAC(R-2R) 등을 연결해 관찰.
 *
 * 구성
 *   TIM2 업데이트 이벤트(10kHz) --DMA 요청--> DMA1 채널2 --> GPIOC->ODR (반워드 전송)
 *   출력 핀: PC0..PC7 (Nucleo morpho). 버퍼 = 64 샘플, 원형 모드라 끝에 닿으면 처음부터 무한 반복.
 *   패턴 주기 = 64 샘플 / 10kHz = 6.4ms -> 약 156Hz 로 반복
 *
 * 핑퐁(double buffering)
 *   DMA 가 버퍼의 앞 절반을 내보내는 동안 CPU 가 뒤 절반을 채우고, 뒤 절반을 내보내는 동안 앞 절반을 채우는 방식.
 *   반 버퍼 전송 완료(HT) / 전체 완료(TC) 인터럽트가 "지금 다 내보낸 쪽이 비었으니 새 데이터를 채우라"는 신호다.
 *   이 예제: 콜백이 비워진 쪽 절반을 "다음 패턴"으로 채운다. 패턴은 1초마다 바뀌는 3종
 *     (0) 나이트라이더(단일 비트 이동) (1) 8비트 이진 카운터 (2) 삼각파(램프 업/다운, 병렬 DAC 에 연결하면 삼각 신호)
 *
 * 하드웨어 대응 (RM0008 13장)
 *   TIM2->DIER.UDE(bit8) = 1 : 업데이트 이벤트마다 DMA 요청
 *   DMA1_Channel2: 메모리->주변장치, 소스 메모리 주소 증가, 목적지(ODR) 고정, 반워드(16비트) 단위, CCR.CIRC=1, HTIE/TCIE
 *   *** GPIOC->ODR 전체(16비트)를 쓰므로 PC8..PC15 는 0 으로 구동된다. PC13(B1)은 입력 모드라 영향이 없지만,
 *       PC8..PC15 에 다른 출력을 쓰는 응용에서는 BSRR 로 필요한 비트만 쓰도록 바꿔야 한다. ***
 *
 * ISR vs 콜백
 *   DMA1_Channel2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> half_cb (HT) / full_cb (TC)  (HAL_DMA_RegisterCallback 으로 등록한 이 파일의 함수)
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
#define N 64                                  /* 전체 샘플 수 (반 = 32) */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
DMA_HandleTypeDef hdma_tim2_up;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint16_t buf[N];
static volatile uint8_t pattern;              /* 0/1/2 : main 이 1초마다 변경 */
static volatile uint32_t half_count, full_count;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 반 버퍼(32개)를 현재 패턴으로 채운다. phase = 이 절반이 패턴 안에서 시작하는 위치 */
static void fill_half(uint16_t *p, uint32_t start)
{
    for (uint32_t i = 0; i < N / 2; i++)
    {
        uint32_t k = start + i;               /* 패턴 안의 샘플 번호 */
        uint16_t v = 0;
        switch (pattern)
        {
        case 0:  v = (uint16_t)(1u << ((k / 4) & 7));  break;                           /* 나이트라이더: 4샘플마다 한 칸 */
        case 1:  v = (uint16_t)((k * 4) & 0xFF);       break;                           /* 이진 카운터(램프) */
        default: { uint32_t t = k & 0x3F; v = (uint16_t)((t < 32 ? t : 63 - t) * 8); }  /* 삼각파 0..248 */
        }
        p[i] = v;
    }
}

/* ------------------------------------------------------------------------- */
/* DMA 콜백 (ISR 문맥): HT = 앞 절반 전송 완료, TC = 뒤 절반 전송 완료                */
/* ------------------------------------------------------------------------- */
static void half_cb(DMA_HandleTypeDef *h) { (void)h; fill_half(&buf[0], 0);          half_count++; }   /* 앞 절반이 비었음 -> 채움 */
static void full_cb(DMA_HandleTypeDef *h) { (void)h; fill_half(&buf[N / 2], N / 2);  full_count++; }   /* 뒤 절반이 비었음 -> 채움 */

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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 앞/뒤 절반을 첫 패턴(pattern=0)으로 미리 채운다 */
  fill_half(&buf[0], 0);
  fill_half(&buf[N / 2], N / 2);

  /* DMA 콜백 등록: HT -> half_cb, TC -> full_cb. HAL_DMA_Start_IT 가 XferHalfCpltCallback 이 NULL 이 아닐 때만
   * CCR.HTIE 를 켜므로 반드시 Start_IT 보다 먼저 등록한다 (HAL_DMA_IRQHandler 가 ISR 안에서 호출) */
  HAL_DMA_RegisterCallback(&hdma_tim2_up, HAL_DMA_XFER_HALFCPLT_CB_ID, half_cb);
  HAL_DMA_RegisterCallback(&hdma_tim2_up, HAL_DMA_XFER_CPLT_CB_ID, full_cb);

  /* DMA 시작: 메모리 buf -> GPIOC->ODR, 원형 (CCR.CIRC=1, MX_TIM2 의 DMA 설정은 HAL_TIM_Base_MspInit). TIM2 업데이트마다 1샘플 */
  if (HAL_DMA_Start_IT(&hdma_tim2_up, (uint32_t)buf, (uint32_t)&GPIOC->ODR, N) != HAL_OK) Error_Handler();
  __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_UPDATE);        /* TIM2->DIER.UDE(bit8) = 1 */
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) Error_Handler();   /* TIM2->CR1.CEN = 1 */

  char msg[96];
  uint32_t t_sw = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GetTick() - t_sw >= 1000)
    {
      t_sw += 1000;
      pattern = (uint8_t)((pattern + 1) % 3);      /* 콜백들이 다음 반 버퍼부터 새 패턴으로 채운다 */
      static const char *const nm[3] = { "knight-rider", "binary ramp", "triangle" };
      int n = snprintf(msg, sizeof msg, "pattern=%s  HT=%lu TC=%lu (기대: 각각 약 156/s)\r\n",
              nm[pattern], (unsigned long)half_count, (unsigned long)full_count);
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
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
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC0 PC1 PC2 PC3 PC4 PC5 PC6 PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 63;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */
  /* TIM2 클럭 64MHz / (PSC+1=64) / (ARR+1=100) = 10kHz 업데이트 이벤트 = DMA 요청 주기.
   * TIM2->PSC = 63, TIM2->ARR = 99. DIER.UDE / CR1.CEN 은 main() USER CODE 2 에서 설정 */
  /* USER CODE END TIM2_Init 2 */

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

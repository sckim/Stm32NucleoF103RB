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
 * 04_TIM_OutputCompare  —  [계층: HAL]  TIM2 출력 비교(Output Compare): Toggle 모드 + Timing 모드
 *
 * 동작 (타이머 하나, 자유 계수 카운터 하나로 세 채널이 서로 다른 일을 한다)
 *   - TIM2 카운터 10kHz(0.1ms), ARR=0xFFFF 자유 계수.
 *   - CH1(PA0) Toggle 모드 : CNT==CCR1 마다 핀 반전, ISR 에서 CCR1 += 2500  -> 0.25s 마다 반전 = 2Hz 깜빡임
 *   - CH2(PA1) Toggle 모드 : CCR2 += gHalf2 (B1 로 1000/500/250/2500 순환) -> 5 / 10 / 20 / 2 Hz
 *   - CH3     Timing 모드 : 핀 출력 없음, CNT==CCR3 마다 인터럽트만. CCR3 += 10000 -> 1초마다 LD2 토글 + UART 보고
 *   - PWM(05)과의 차이: PWM 은 모든 채널이 ARR(=주파수)를 공유한다. OC + CCR 전진 방식은 채널마다 주파수가 다르다.
 *
 * 배선: PA0(A0) ── 330Ω ── LED1 ── GND,   PA1(A1) ── 330Ω ── LED2 ── GND   (05_TIM_PWM 과 같은 배선)
 *
 * 타이머 계산
 *   TIM2 클럭 64MHz,  PSC=6399 -> 10kHz (0.1ms),  ARR=65535 -> 오버플로 주기 6.5536s
 *   Toggle 출력 주파수 f = 10kHz / (2 x HALF)      예) HALF=2500 -> 2Hz
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler() 가 SR.CCxIF 를 지움
 *     -> HAL_TIM_OC_DelayElapsedCallback() (이 파일) : htim->Channel 로 채널을 구분해 CCRx 를 다음 시각으로 전진
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define OC_TICK_HZ     10000U   /* 카운터 클럭 = 64MHz / (PSC+1) = 64MHz / 6400 */
#define CH1_HALF       2500U    /* CH1 반주기 [tick] = 0.25s -> 2Hz */
#define CH3_INTERVAL   10000U   /* CH3 이벤트 간격 [tick] = 1s */
#define B1_DEBOUNCE_MS 50U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* CH2 반주기 후보 [tick]: 5Hz -> 10Hz -> 20Hz -> 2Hz -> ... */
static const uint16_t kHalf2Table[] = { 1000U, 500U, 250U, 2500U };
static volatile uint16_t gHalf2 = 1000U;   /* main 이 쓰고 ISR 이 읽는다 (16비트 쓰기는 원자적) */
static volatile uint32_t gSec;             /* CH3 이벤트 횟수 = 경과 초 */
static volatile uint8_t  gSecFlag;         /* CH3 이벤트 -> main 루프에 보고 요청 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void PrintFreq(const char *name, uint16_t half);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * printf 리타깃 (HAL 계층)
 *   printf -> newlib _write (syscalls.c) -> __io_putchar(ch) -> HAL_UART_Transmit (블로킹)
 */
int __io_putchar(int ch) {
	if (ch == '\n') {
		uint8_t cr = '\r';
		HAL_UART_Transmit(&huart2, &cr, 1, HAL_MAX_DELAY);
	}
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
	return ch;
}

/* Toggle 출력 주파수 = OC_TICK_HZ / (2 x half), 소수 첫째 자리까지 정수 연산으로 출력 */
static void PrintFreq(const char *name, uint16_t half)
{
	uint32_t f10 = (OC_TICK_HZ * 10U) / (2U * half);   /* 주파수 x10 */
	printf("%s %2lu.%lu Hz", name, (unsigned long)(f10 / 10U), (unsigned long)(f10 % 10U));
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint8_t  half2Idx = 0;
  uint8_t  prevB1 = 1;          /* B1 은 눌리면 0 (외부 풀업) */
  uint32_t lastB1Tick = 0;
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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("\nTIM2 Output Compare start (10kHz tick, CH1/CH2 toggle, CH3 timing)\n");

  /* CH3 = Timing 모드: 비교 인터럽트만 켠다 (DIER.CC3IE=1).
   * HAL_TIM_OC_Start_IT(CH3) 를 쓰지 않는 이유: 이 함수는 CCER.CC3E=1 로 출력까지 켠다.
   * TIM2_CH3 의 핀은 PA2 = USART2_TX 이므로, F1 에서 두 주변장치가 같은 AF 핀을 구동하면 충돌한다. */
  __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_CC3);

  /* CH1, CH2 = Toggle 모드: DIER.CCxIE=1, CCER.CCxE=1, CR1.CEN=1 */
  if (HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
  if (HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

  /* 레지스터 확인 (기대값: CCMR1=0x3030 OC1M/OC2M=011 Toggle, CCMR2=0x0000 OC3M=000 Frozen,
   *               CCER=0x0011 CC1E/CC2E, DIER=0x000E CC1IE/CC2IE/CC3IE) */
  printf("CCMR1=0x%04lX CCMR2=0x%04lX CCER=0x%04lX DIER=0x%04lX\n",
         (unsigned long)TIM2->CCMR1, (unsigned long)TIM2->CCMR2,
         (unsigned long)TIM2->CCER,  (unsigned long)TIM2->DIER);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ---- B1(PC13) 하강 에지: CH2 반주기 변경 (폴링 + 디바운스) ---- */
    uint8_t b1 = (uint8_t)HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);   /* GPIOC->IDR bit13 */
    if (prevB1 == 1U && b1 == 0U && (HAL_GetTick() - lastB1Tick) > B1_DEBOUNCE_MS)
    {
      lastB1Tick = HAL_GetTick();
      half2Idx = (uint8_t)((half2Idx + 1U) % (sizeof kHalf2Table / sizeof kHalf2Table[0]));
      gHalf2 = kHalf2Table[half2Idx];          /* 다음 CH2 비교부터 적용된다 */
      printf("B1: ");
      PrintFreq("CH2(PA1)", gHalf2);
      printf("\n");
    }
    prevB1 = b1;

    /* ---- CH3 이벤트(1초)마다 보고 ---- */
    if (gSecFlag)
    {
      gSecFlag = 0;
      printf("[%3lu s] ", (unsigned long)gSec);
      PrintFreq("CH1(PA0)", CH1_HALF);
      printf("  ");
      PrintFreq("CH2(PA1)", gHalf2);
      printf("  | CNT=%5lu CCR1=%5lu CCR2=%5lu CCR3=%5lu\n",
             (unsigned long)__HAL_TIM_GET_COUNTER(&htim2),
             (unsigned long)__HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1),
             (unsigned long)__HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_2),
             (unsigned long)__HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_3));
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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 6399;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
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
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
  sConfigOC.Pulse = 2500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.Pulse = 1000;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 10000;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */
  /* PSC=6400-1 -> 10kHz, ARR=0xFFFF 자유 계수, ARPE=0.
   * CH1/CH2: CCMR1.OCxM=011(Toggle), CH3: CCMR2.OC3M=000(Frozen=Timing).
   * HAL_TIM_OC_ConfigChannel 은 OCxPE(프리로드)를 켜지 않는다 -> ISR 에서 쓴 CCRx 가 즉시 다음 비교값이 된다. */
  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥, HAL_TIM_IRQHandler 가 SR.CCxIF 를 지운 뒤 호출)            */
/*   CCR 전진: 다음 비교 시각 = 이번 비교 시각 + 간격.                           */
/*   ARR=0xFFFF 이므로 uint16_t 덧셈의 래핑이 카운터 래핑과 정확히 맞는다.         */
/* ------------------------------------------------------------------------- */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance != TIM2) return;

  switch (htim->Channel)
  {
  case HAL_TIM_ACTIVE_CHANNEL_1:   /* 핀 반전은 하드웨어가 이미 했다. 다음 반전 시각만 정한다 */
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1,
        (uint16_t)(__HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_1) + CH1_HALF));   /* TIM2->CCR1 += 2500 */
    break;

  case HAL_TIM_ACTIVE_CHANNEL_2:
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2,
        (uint16_t)(__HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_2) + gHalf2));     /* TIM2->CCR2 += gHalf2 */
    break;

  case HAL_TIM_ACTIVE_CHANNEL_3:   /* Timing 모드: 핀 변화 없음, "정해진 시각의 인터럽트"로만 쓴다 */
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3,
        (uint16_t)(__HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_3) + CH3_INTERVAL)); /* TIM2->CCR3 += 10000 */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);   /* 짧은 GPIO 작업은 ISR 에서 해도 된다 */
    gSec++;
    gSecFlag = 1;                                  /* printf 는 main 루프에서 */
    break;

  default:
    break;
  }
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

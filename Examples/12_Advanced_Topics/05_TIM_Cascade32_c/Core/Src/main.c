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
 * 05_TIM_Cascade32_c  —  [계층: HAL]  타이머 마스터/슬레이브 연결로 만든 32비트 카운터 (스톱워치 71분)
 *
 * F103 의 범용 타이머는 16비트라 1MHz(1us)로 세면 65.5ms 마다 넘친다. 타이머 두 개를 **연결**(cascade)하면 32비트가 된다.
 *
 * 구조 (RM0008 15.3.15 Timer synchronization)
 *   TIM3 = 마스터: PSC = 64-1 (1MHz), ARR = 0xFFFF. 넘칠 때(업데이트 이벤트) **TRGO 로 트리거 출력** (CR2.MMS = 010)
 *   TIM2 = 슬레이브: **외부 클럭 모드 1**(SMCR.SMS = 111)에서 트리거 입력 **ITR2**(= TIM3 의 TRGO, SMCR.TS = 010)를 클럭으로 사용
 *          -> TIM3 가 65536 번 세고 넘칠 때마다 TIM2 가 1 증가한다.
 *   32비트 값 = (TIM2->CNT << 16) | TIM3->CNT  -> 1us 분해능으로 최대 2^32 us = 약 71.6분.
 *   (TIM2 의 ITRx 매핑은 마스터 타이머에 따라 다르다: ITR0=TIM1, ITR1=TIM8(없음), ITR2=TIM3, ITR3=TIM4. RM0008 표 참고)
 *
 * 읽기 경합 주의: 두 카운터를 순서대로 읽는 사이 TIM3 가 넘치면 값이 튈 수 있다.
 *   -> 상위(TIM2)-하위(TIM3)-상위(TIM2) 순으로 읽어 상위가 바뀌었으면 다시 읽는다.
 *
 * 동작
 *   - 1초마다 32비트 시각(us)과 HAL_GetTick(ms)을 비교 출력 -> 거의 1,000,000us 씩 증가하는지 확인 (같은 64MHz 클럭 기반이라 오차 작음)
 *   - B1(PC13): 스톱워치. 첫 번째 누름 = 시작, 두 번째 누름 = 정지하고 경과 시간(us -> h:m:s.ms)을 출력, 세 번째 = 다시 시작
 *
 * 인터럽트를 사용하지 않는다.
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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* htim2 = 슬레이브(상위 16비트), htim3 = 마스터(하위 16비트) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 32비트 마이크로초 카운터를 경합 없이 읽는다 */
static uint32_t micros32(void)
{
    uint32_t hi1, lo, hi2;
    do {
        hi1 = TIM2->CNT;
        lo  = TIM3->CNT;
        hi2 = TIM2->CNT;
    } while (hi1 != hi2);                   /* 읽는 도중 하위가 넘쳐 상위가 바뀌었으면 재시도 */
    return (hi2 << 16) | lo;
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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 슬레이브(TIM2)를 먼저 켜고(대기), 마스터(TIM3)를 켠다: TIM2->CR1.CEN, TIM3->CR1.CEN */
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) Error_Handler();
  if (HAL_TIM_Base_Start(&htim3) != HAL_OK) Error_Handler();

  char msg[96];
  uint32_t t_us0 = micros32(), t_ms0 = HAL_GetTick();
  uint32_t t_print = HAL_GetTick();
  int running = 0;
  uint32_t sw_start = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GetTick() - t_print >= 1000)
    {
        t_print += 1000;
        uint32_t us = micros32(), ms = HAL_GetTick();
        int n = snprintf(msg, sizeof msg, "micros32=%10lu (d=%lu us)  HAL tick=%lu ms (d=%lu ms)\r\n",
                         (unsigned long)us, (unsigned long)(us - t_us0), (unsigned long)ms, (unsigned long)(ms - t_ms0));
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        t_us0 = us; t_ms0 = ms;
    }

    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* B1: 스톱워치 시작/정지 */
    {
        HAL_Delay(30);
        if (!running)
        {
            sw_start = micros32();
            running = 1;
            HAL_UART_Transmit(&huart2, (uint8_t *)"stopwatch: START\r\n", 18, 100);
        }
        else
        {
            uint32_t el = micros32() - sw_start;                  /* 부호 없는 뺄셈: 71분 래핑에도 안전 */
            running = 0;
            uint32_t ms = el / 1000UL, s = ms / 1000UL, m = s / 60UL;
            int n = snprintf(msg, sizeof msg, "stopwatch: STOP  %lu us  = %lu:%02lu.%03lu (m:s.ms)\r\n",
                             (unsigned long)el, (unsigned long)m, (unsigned long)(s % 60UL), (unsigned long)(ms % 1000UL));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */


  /* USER CODE END TIM2_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */


  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
  sSlaveConfig.InputTrigger = TIM_TS_ITR2;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
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
  /* [슬레이브 TIM2 -- 레지스터 관점] 상위 16비트 카운터. 자체 분주 없음(PSC=0), ARR=0xFFFF
   *  TIM2->SMCR.SMS = 111 (외부 클럭 모드 1), SMCR.TS = 010 (트리거 입력 ITR2 = TIM3 의 TRGO)
   *  -> TIM3 가 65536 번 세고 넘칠 때마다 TIM2 가 1 증가한다.
   *  (TIM2 의 ITRx 매핑: ITR0=TIM1, ITR1=TIM8(없음), ITR2=TIM3, ITR3=TIM4. RM0008 표 참고) */
  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */


  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */


  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 63;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  /* [마스터 TIM3 -- 레지스터 관점] 하위 16비트 카운터. PSC=63 (64MHz/64 = 1MHz = 1us), ARR=0xFFFF
   *  TIM3->CR2.MMS = 010 : 업데이트 이벤트(넘침)를 TRGO 로 출력 -> 슬레이브 TIM2 의 클럭 */
  /* USER CODE END TIM3_Init 2 */

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

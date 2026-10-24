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
 * 06_NVIC_Priority_c  —  [계층: HAL]  NVIC 우선순위 그룹(선점/서브)과 인터럽트 중첩 실험
 *
 * 실험 구성
 *   TIM3 ISR : 10ms 마다 발생, 짧다. 진입 간격의 "최대값(지터)"을 측정한다. (급한 인터럽트 역할)
 *   TIM2 ISR : 100ms 마다 발생, 일부러 20ms 동안 바쁜 대기한다. (오래 걸리는 인터럽트 역할)
 *   -> TIM2 ISR 이 실행되는 20ms 동안 TIM3 가 제때 들어올 수 있는지가 우선순위 설정에 달렸다.
 *
 * B1(PC13)을 누를 때마다 우선순위 케이스가 A -> B -> C 로 바뀌고, 1초마다 결과를 UART(115200)로 출력한다.
 *
 *   케이스 A (TIM3 선점 우선순위 0 < TIM2 1) : TIM3 가 TIM2 ISR 을 "선점(중첩)". 최대 간격 ~10ms 유지.
 *   케이스 B (선점 우선순위 같음 1, 서브만 TIM3 가 높음) : 서브 우선순위는 "선점하지 못한다".
 *                                              TIM3 는 TIM2 ISR 이 끝날 때까지 대기 -> 최대 간격 ~20~30ms.
 *   케이스 C (TIM3 선점 2, TIM2 선점 1) : 긴 ISR 에 더 높은 우선순위를 준 잘못된 배정. B 와 비슷하게 지연.
 *   교훈: "짧고 급한" 인터럽트에 높은 우선순위를, "길고 덜 급한" 인터럽트에 낮은 우선순위를 주어야 한다.
 *
 * NVIC 우선순위 (STM32F103: 우선순위 비트 4개 = 상위 4비트만 구현)
 *   HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2) -> SCB->AIRCR.PRIGROUP = 5
 *     -> 우선순위 필드 [7:4] 중 상위 2비트 = 선점(preempt, 0~3), 하위 2비트 = 서브(sub, 0~3)
 *   HAL_NVIC_SetPriority(irq, preempt, sub) -> NVIC->IP[irq] 에 인코딩하여 기록. 숫자가 작을수록 우선순위가 높다.
 *   같은 선점 우선순위끼리는 중첩 불가, 동시에 대기 중일 때만 서브 우선순위로 실행 순서를 정한다.
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler / TIM3_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler()
 *     -> HAL_TIM_PeriodElapsedCallback() (이 파일) : 두 타이머가 같은 콜백을 공유하므로 Instance 로 구분
 *   ISR 안에서 UART 출력을 하지 않고 통계 변수만 갱신한다 (출력은 main).
 *
 * 측정: DWT 사이클 카운터 (64MHz). SysTick 은 TICK_INT_PRIORITY=0(최고)이라 HAL_Delay 가 흔들리지 않는다.
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
#define CPU_MHZ 64U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint32_t tim3_last;         /* TIM3 직전 진입 시각 (사이클) */
static volatile uint32_t tim3_max_gap_us;   /* 최근 1초간 TIM3 진입 간격 최대값 [us] */
static volatile uint32_t tim3_count, tim2_count;
static volatile uint8_t  tim3_first = 1;

static uint8_t prio_case;                   /* 0=A, 1=B, 2=C */

static const char *const case_desc[3] = {
    "A: TIM3(pre 0) preempts TIM2(pre 1)  -> nesting",
    "B: same preempt(1), sub only         -> no nesting",
    "C: TIM3(pre 2) below TIM2(pre 1)     -> long ISR wins"
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void apply_case(uint8_t c);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 케이스별 NVIC 우선순위 적용. NVIC->IP[TIM2_IRQn / TIM3_IRQn] 를 다시 쓴다 */
static void apply_case(uint8_t c)
{
    static const uint8_t tim3_pre[3] = {0, 1, 2}, tim3_sub[3] = {0, 0, 0};
    static const uint8_t tim2_pre[3] = {1, 1, 1}, tim2_sub[3] = {0, 1, 0};

    HAL_NVIC_SetPriority(TIM3_IRQn, tim3_pre[c], tim3_sub[c]);
    HAL_NVIC_SetPriority(TIM2_IRQn, tim2_pre[c], tim2_sub[c]);

    __disable_irq();
    prio_case = c;
    tim3_first = 1; tim3_max_gap_us = 0; tim3_count = 0; tim2_count = 0;
    __enable_irq();
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
  /* NVIC 우선순위 그룹 2: SCB->AIRCR.PRIGROUP = 5 -> 선점 2비트 / 서브 2비트.
   * HAL_MspInit/MX_TIM*_Init (TIMx_MspInit 의 HAL_NVIC_SetPriority) 보다 먼저 설정해야 우선순위가 이 그룹 기준으로 인코딩된다. */
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);
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
  /* DWT 사이클 카운터: CoreDebug->DEMCR.TRCENA=1, DWT->CTRL.CYCCNTENA=1 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* 케이스 A 우선순위 적용 (NVIC->IP[TIM2_IRQn/TIM3_IRQn] 를 선점/서브 그룹 2 기준으로 기록) 후 타이머 시작.
   * 원본은 타이머를 먼저 시작한 뒤 apply_case(0) 를 호출했지만, 우선순위가 정해진 뒤에 시작하도록 순서를 바꿨다. */
  apply_case(0);
  if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK) Error_Handler();   /* TIM3->DIER.UIE = 1, CR1.CEN = 1 */
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) Error_Handler();   /* TIM2->DIER.UIE = 1, CR1.CEN = 1 */

  char msg[112];
  uint32_t t_print = DWT->CYCCNT;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 1초마다 통계 출력 (SysTick 이 아니라 DWT 로 시간을 잰다) */
    if ((DWT->CYCCNT - t_print) >= CPU_MHZ * 1000000U)
    {
      t_print += CPU_MHZ * 1000000U;

      __disable_irq();                               /* ISR 이 쓰는 변수를 한꺼번에 읽고 초기화 */
      uint32_t gap = tim3_max_gap_us, c3 = tim3_count, c2 = tim2_count;
      tim3_max_gap_us = 0; tim3_count = 0; tim2_count = 0;
      __enable_irq();

      int n = snprintf(msg, sizeof msg, "[%s] TIM3 max gap=%lu us, TIM3=%lu, TIM2=%lu (1s)\r\n",
              case_desc[prio_case], (unsigned long)gap, (unsigned long)c3, (unsigned long)c2);
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
    }

    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* B1 : 케이스 전환 */
    {
      HAL_Delay(30);
      apply_case((uint8_t)((prio_case + 1) % 3));
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
  htim2.Init.Prescaler = 6399;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
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
  /* 타이머 클럭 64MHz / 6400 = 10kHz.  TIM2: ARR = 999 -> 100ms 주기 (길고 덜 급한 인터럽트) */
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
  htim3.Init.Prescaler = 6399;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  /* 타이머 클럭 64MHz / 6400 = 10kHz.  TIM3: ARR = 99 -> 10ms 주기 (짧고 급한 인터럽트) */
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

/* USER CODE BEGIN 4 */
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 두 타이머의 주기 인터럽트가 같은 콜백으로 들어온다              */
/* ------------------------------------------------------------------------- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)                            /* 짧고 급한 인터럽트: 진입 간격 측정 */
    {
        uint32_t now = DWT->CYCCNT;
        if (!tim3_first)
        {
            uint32_t gap_us = (now - tim3_last) / CPU_MHZ;
            if (gap_us > tim3_max_gap_us) tim3_max_gap_us = gap_us;
        }
        tim3_first = 0;
        tim3_last = now;
        tim3_count++;
    }
    else if (htim->Instance == TIM2)                       /* 길고 덜 급한 인터럽트: 20ms 점유 */
    {
        uint32_t t = DWT->CYCCNT;
        while ((DWT->CYCCNT - t) < CPU_MHZ * 20000U) {}    /* 20ms 바쁜 대기 (실제 코드에서는 절대 금지) */
        tim2_count++;
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

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
 * 04_Link_Clock_Counter_HAL_c  —  [계층: HAL]  TIM1_CH2(PA9=D8)에 256 Hz 클럭(주기 1/256 s, 듀티 50 %)을 출력
 *                                 (DE1-SoC 연동 실습 4: FPGA 가 이 클럭의 상승 엣지를 세어 LEDR0~9 와 HEX0~5 에 표시)
 *
 * 기반: 03_Link_PWM_Meter_HAL_c. 출력 채널을 CH1(PA8)에서 CH2(PA9)로 옮기고 주파수를 1 Hz 로 낮췄다.
 *   .ioc 와 main.c 의 CubeMX 생성 영역, 그리고 stm32f1xx_hal_msp.c / main.h 의 핀 정의를 함께 고쳤다.
 *
 * 동작
 *   TIM1 이 하드웨어로 클럭 파형을 만든다. CPU 는 시작만 시키고 버튼 B1(PC13)을 폴링한다.
 *   B1 을 누르는 동안 클럭이 1 Hz(PSC=64000-1), 떼면 256 Hz(PSC=250-1)로 돌아온다.
 *   FPGA(fpga/lab4_top.v)는 이 클럭의 상승 엣지마다 24비트 카운터를 1 올려
 *   LEDR0~9 에 하위 10비트(2진수), HEX5~HEX0 에 24비트 전체(16진수 6자리)를 보여 준다.
 *
 * 배선: D8 (PA9, TIM1_CH2) -> DE1-SoC JP1 1번 (GPIO_0[0], PIN_AC18),  GND 공통(JP1 12번)
 *   PA10(D2)은 쓰지 않으므로 연결하지 않는다.
 *
 * 클럭 주파수 계산
 *   TIM1 클럭 = 64MHz (APB2=64MHz, 분주비 == 1 이므로 타이머 클럭 x1)
 *   PSC = 250-1   -> 카운터 클럭 256kHz, 1카운트 = 3.90625 us  (TIM1->PSC)
 *   ARR = 1000-1  -> 주기 1000 카운트 = 1/256 s = 256Hz       (TIM1->ARR)
 *   CCR2 = 500    -> High 절반, Low 절반 (듀티 50 %)          (TIM1->CCR2)
 *   CCMR1.OC2M = 110 (PWM mode 1: CNT < CCR2 이면 High), CCER.CC2E = 1 (출력 활성)
 *   주파수 = 64MHz / ((PSC+1) x (ARR+1)). 예) PSC = 64000-1 -> 1 Hz, PSC = 6400-1 -> 10 Hz
 *
 * TIM1 은 Advanced 타이머 : 출력이 나오려면 BDTR.MOE = 1 이어야 한다.
 *   HAL_TIM_PWM_Start() 가 내부에서 __HAL_TIM_MOE_ENABLE() 로 MOE 를 세트한다.
 *
 * 인터럽트/콜백: 쓰지 않는다 (파형은 타이머 하드웨어가 생성). ISR 은 SysTick_Handler(HAL 시간 기준)뿐이다.
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
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
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  /* 클럭 출력 시작: CCER.CC2E = 1 (CH2 출력 활성), BDTR.MOE = 1 (TIM1 메인 출력 활성),
     CR1.CEN = 1 (카운터 시작) */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
  uint8_t slow = 0;   /* 0: 256 Hz (버튼 뗌), 1: 1 Hz (버튼 누름) */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 클럭 파형은 TIM1 하드웨어가 만든다. CPU 는 버튼 B1(PC13, 누르면 Low)만 폴링한다.
       누르는 동안: PSC = 64000-1 -> 1 Hz (주기 1 s)
       떼면      : PSC = 250-1   -> 256 Hz (주기 1/256 s)
       TIM1->PSC 는 프리로드(버퍼) 레지스터라 다음 업데이트 이벤트(UEV)에서야 적용된다.
       1 Hz 에서 256 Hz 로 돌아올 때 최대 1 s 를 기다리지 않도록, 값을 바꾼 직후
       EGR.UG = 1 (HAL_TIM_GenerateEvent)로 UEV 를 강제해 즉시 적용한다(CNT 도 0 으로 초기화).
       버튼 상태가 바뀐 때만 한 번 갱신한다.
       (폴링이므로 ISR/콜백은 없다. 인터럽트는 SysTick_Handler 뿐이다.) */
    uint8_t pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
    if (pressed != slow)
    {
      slow = pressed;
      __HAL_TIM_SET_PRESCALER(&htim1, slow ? 63999 : 249);   /* TIM1->PSC */
      HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE); /* TIM1->EGR.UG */
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 249;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  /* PSC=250-1 -> 256kHz, ARR=1000-1 -> 256Hz (주기 1/256 s), CCR2=500 -> 듀티 50 % (OC2M=110 PWM1, CR1.ARPE=1)
     BDTR: DTG=0, BKE=0, AOE=0 -> MOE 는 HAL_TIM_PWM_Start() 에서 세트 */
  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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

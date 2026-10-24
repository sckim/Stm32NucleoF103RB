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
 * 10_TIM_MotorDriver_TB6612_c  —  [계층: HAL]  H-브리지 모터 드라이버(TB6612FNG)로 DC 모터: PWM 속도 + 방향 핀 + 정지 방식
 *
 * TB6612FNG 채널 A 제어 (PWMA, AIN1, AIN2, STBY)
 *   AIN1 AIN2 | 동작
 *    H    L   | 정회전 (PWMA 듀티가 속도)
 *    L    H   | 역회전
 *    H    H   | **쇼트 브레이크**: 모터 단자를 서로 단락 -> 빠르게 멈춘다 (역기전력이 모터를 제동)
 *    L    L   | **코스트(정지, 자유 회전)**: 출력 Hi-Z -> 관성으로 서서히 멈춘다
 *   STBY = Low 이면 전체 대기(출력 차단), High 여야 동작.
 *   PWM 주파수: 20kHz 로 가청 대역(약 20kHz) 위로 올려 모터 "삐-" 소리를 피한다. (TB6612 최대 100kHz)
 *
 * 배선 (Nucleo-F103RB)
 *   PWMA = PA0 (TIM2_CH1),  AIN1 = PB0,  AIN2 = PB1,  STBY = PB2
 *   VM = 모터 전원(4.5~13.5V, **외부 전원**),  VCC = 3.3V(로직),  GND = 보드 GND 와 **공통**,  AO1/AO2 = 모터
 *   *** 모터 전원을 Nucleo 에서 끌어 쓰지 말 것. 기동 전류가 보드를 리셋시킨다. 모터 양단에 역기전력 흡수를 위한 캐패시터/다이오드 권장. ***
 *
 * PWM 계산: TIM2 64MHz, PSC = 0, ARR = 3200-1 -> 20kHz, CCR1 = 0..3200 (속도 -1000..+1000 의 절댓값을 0..3200 으로 스케일)
 *
 * 동작 시퀀스 (반복, UART 로 단계 출력)
 *   1) 정회전 가속 0 -> 100% (2초)   2) 쇼트 브레이크 0.5초   3) 역회전 가속 0 -> 100% (2초)   4) 코스트 1초(자연 정지)
 *   B1(PC13)을 누르면 즉시 **브레이크 + STBY Low**(비상 정지). 다시 누르면 재개.
 *   *** 이 예제는 단계마다 급격한 방향 전환을 하지 않는다: 반드시 속도를 0 으로 줄이거나 브레이크를 거쳐 방향을 바꾼다
 *       (달리는 모터에 즉시 반대 방향 전압을 주면 큰 전류가 흘러 드라이버/전원에 무리가 간다). ***
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
#define PWM_TOP 3200U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

static void set_dir(int in1, int in2)
{
    HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, in1 ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* AIN1 = PB0 (GPIOB->BSRR/BRR bit0) */
    HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, in2 ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* AIN2 = PB1 (GPIOB->BSRR/BRR bit1) */
}

/* speed: -1000(역 최대) .. 0 .. +1000(정 최대) */
static void motor_set(int speed)
{
    if (speed > 1000) speed = 1000;
    if (speed < -1000) speed = -1000;
    if (speed > 0)       set_dir(1, 0);
    else if (speed < 0)  set_dir(0, 1);
    else                 set_dir(0, 0);                                      /* 0 은 코스트 */
    uint32_t mag = (uint32_t)(speed < 0 ? -speed : speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, mag * PWM_TOP / 1000U);      /* TIM2->CCR1 */
}

static void motor_brake(void)   { __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, PWM_TOP); set_dir(1, 1); }   /* 쇼트 브레이크 (PWM 100%) */
static void motor_coast(void)   { __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); set_dir(0, 0); }          /* 자유 회전 */
static void standby(int on)     { HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET); }  /* STBY(PB2) Low = 대기 */

/* 지정 시간 동안 대기하면서 B1 비상 정지를 감시. 눌렸으면 1 반환 */
static int wait_or_estop(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < ms)
        if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) return 1;   /* GPIOC->IDR bit13 */
    return 0;
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
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();   /* TIM2->CCER.CC1E = 1, CR1.CEN = 1 */
  standby(0);                                                              /* 출력 활성 (STBY High) */
  motor_coast();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    int estop = 0;

    print("forward ramp 0 -> 100%\r\n");
    for (int s = 0; s <= 1000 && !estop; s += 10) { motor_set(s); estop = wait_or_estop(20); }

    if (!estop) { print("short brake\r\n"); motor_brake(); estop = wait_or_estop(500); }

    if (!estop) { print("reverse ramp 0 -> 100%\r\n"); for (int s = 0; s <= 1000 && !estop; s += 10) { motor_set(-s); estop = wait_or_estop(20); } }

    if (!estop) { print("coast (free spin)\r\n"); motor_coast(); estop = wait_or_estop(1000); }

    if (estop)
    {
      motor_brake();
      HAL_Delay(300);
      standby(1);                                                      /* STBY Low: 출력 차단 */
      print("*** E-STOP: brake + standby. release B1 and press again to resume\r\n");
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}   /* 뗄 때까지 */
      HAL_Delay(50);
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) {}     /* 다시 누를 때까지 */
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
      HAL_Delay(50);
      standby(0);
      motor_coast();
      print("resume\r\n");
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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, AIN1_Pin|AIN2_Pin|STBY_Pin, GPIO_PIN_RESET);

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

  /*Configure GPIO pins : AIN1_Pin AIN2_Pin STBY_Pin */
  GPIO_InitStruct.Pin = AIN1_Pin|AIN2_Pin|STBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3199;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */
  /* TIM2: PSC=0 (64MHz), ARR = PWM_TOP-1 = 3199 -> 64MHz/3200 = 20kHz, CCR1 = 0..3200, CR1.ARPE = 1 */
  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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

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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/*
 * 이 예제에는 TIM3 ISR 도, HAL_TIM_PeriodElapsedCallback 도 없다.
 * 아래 변수는 main 루프에서만 읽고 쓰므로 volatile 이 필요 없다.
 */
uint32_t gTimerCnt;              /* 폴링으로 확인한 1ms 업데이트 횟수 (0~999) */
uint32_t gSeconds;               /* 경과 시간 [s] - UIF 1000회마다 1 증가 */
uint32_t gTickStart;             /* 타이머 시작 시점의 HAL_GetTick() [ms] (비교 기준) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * printf 리타깃 (HAL 계층)
 *   printf -> newlib _write (syscalls.c) -> __io_putchar(ch) -> HAL_UART_Transmit
 *   USART2(PA2 TX, 115200 8N1)는 ST-LINK 가상 COM 포트로 연결되어 PC 터미널에 출력된다.
 *   HAL_UART_Transmit 은 USART2_SR.TXE 를 폴링하며 USART2_DR 에 1바이트씩 쓰는 블로킹 함수이다.
 *   '\n' 앞에 '\r' 을 추가 전송해 터미널에서 줄 바꿈이 맞게 보이게 한다.
 */
int __io_putchar(int ch) {
	if (ch == '\n') {
		uint8_t cr = '\r';
		HAL_UART_Transmit(&huart2, &cr, 1, HAL_MAX_DELAY);
	}
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
	return ch;
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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
	printf("TIM3 TimeBase start (1ms update, UIF polling, print every 1s)\n");

	/*
	 * 시작 전 UIF 정리
	 *   HAL_TIM_Base_Init() 은 TIM3_EGR.UG 로 PSC/ARR 을 즉시 반영하는데, 이때 TIM3_SR.UIF 가
	 *   세트될 수 있다. (현재 HAL 버전은 Init 안에서 지워 주지만 버전에 따라 다르다.)
	 *   남은 UIF 를 지우지 않으면 첫 1ms 를 공짜로 세게 되므로 시작 직전에 명시적으로 지운다.
	 *   TIM3_SR.UIF(bit0) 는 rc_w0 비트: 0 을 써야 지워진다 -> __HAL_TIM_CLEAR_FLAG 는 SR = ~UIF 를 쓴다.
	 */
	__HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
	gTickStart = HAL_GetTick();

	/*
	 * HAL_TIM_Base_Start (인터럽트 없는 버전)
	 *   TIM3_CR1.CEN = 1 (카운터 시작) 만 한다.
	 *   TIM3_DIER.UIE 는 0 그대로이므로 오버플로 때 UIF 플래그만 서고 TIM3_IRQn 은 발생하지 않는다.
	 *   (01_TIM_TimeBase 의 HAL_TIM_Base_Start_IT 는 여기에 DIER.UIE = 1 을 더한다.)
	 */
	if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
		Error_Handler();
	}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		/*
		 * 업데이트 플래그 폴링 (main 루프 문맥, ISR 아님)
		 *   TIM3_SR.UIF(bit0): CNT 가 ARR(999) 에서 0 으로 넘어갈 때(1ms 마다) 하드웨어가 1 로 세운다.
		 *   __HAL_TIM_GET_FLAG 는 TIM3->SR & TIM_SR_UIF 를 읽기만 한다.
		 *
		 *   주의 1) 확인하자마자 먼저 지운다.
		 *     작업(printf 등)을 마친 뒤 지우면, 작업 도중 새로 선 UIF 까지 함께 지워 1회를 잃는다.
		 *   주의 2) UIF 는 1비트라 횟수를 쌓아 두지 못한다.
		 *     루프 한 바퀴가 1ms 를 넘기면, 그 사이 여러 번 선 UIF 가 1회로 합쳐져 틱을 놓친다.
		 */
		if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) != RESET) {
			__HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);   /* TIM3_SR.UIF = 0 */

			gTimerCnt++;
			if (gTimerCnt == 1000) {                         /* 소프트웨어 분주: 1ms x 1000 = 1s */
				gTimerCnt = 0;
				HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);  /* GPIOA_ODR.ODR5 반전 */
				gSeconds++;

				/*
				 * 경과 시간 출력 + 누락 확인
				 *   이 printf 는 약 30자 x 87us(115200bps) = 2.6ms 동안 루프를 붙잡는다.
				 *   그동안 UIF 가 2~3번 서지만 1번으로만 보이므로 매초 1~2ms 씩 틱을 놓친다.
				 *   SysTick(HAL_GetTick) 은 인터럽트로 세므로 놓치지 않는다 -> 두 값의 차이(lag)가
				 *   초마다 커지는 것으로 폴링 방식의 한계를 확인한다.
				 */
				uint32_t elapsed = HAL_GetTick() - gTickStart;
				printf("[%lu s] tick=%lu ms lag=%ld ms\n", (unsigned long) gSeconds,
						(unsigned long) elapsed, (long) (elapsed - gSeconds * 1000U));
			}
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
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/*
 * HAL_TIM_PeriodElapsedCallback 은 정의하지 않는다.
 *   이 콜백은 TIM3_IRQHandler(ISR) -> HAL_TIM_IRQHandler() 경로로만 불리는데, 이 예제는
 *   TIM3_DIER.UIE = 0, NVIC TIM3_IRQn 비활성(.ioc / stm32f1xx_hal_msp.c)이라 ISR 자체가 없다.
 *   UIF 확인·클리어는 main 루프(USER CODE 3)가 직접 한다.
 *   단, SysTick_Handler(HAL_GetTick 용)는 여전히 인터럽트로 동작한다.
 */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

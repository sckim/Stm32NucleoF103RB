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
 * 03_TIM_Overflow_Counter  —  [계층: HAL + 레지스터 직접 읽기(CNT, SR)]
 *   TIM2 오버플로 횟수로 16비트 카운터를 32비트 µs 시계로 확장한다.
 *
 * 동작
 *   - TIM2 를 1MHz(1us)로 자유 계수(ARR=0xFFFF)시킨다. 65.536ms 마다 오버플로(업데이트 이벤트)가 난다.
 *   - 업데이트 인터럽트에서 gOvf 를 1 증가 -> 시간[us] = (gOvf << 16) | CNT  (32비트, 약 71.6분에 한 바퀴)
 *   - 1초 동안 두 가지 읽기 함수를 계속 호출하며 "시간이 거꾸로 간 횟수"를 센다.
 *       Micros_Naive() : gOvf 와 CNT 를 그냥 차례로 읽는다  -> 경쟁 조건(race)으로 가끔 65536us 뒤로 튄다
 *       Micros_Safe()  : 인터럽트를 잠시 막고 UIF 를 한 번 더 확인한다 -> 뒤로 튀지 않는다
 *   - 매초 USART2(115200)로 결과와 "직전 printf 한 줄의 실행 시간[us]", "HAL_Delay(10) 실측[us]"을 출력한다.
 *   - LD2(PA5)는 1초마다 토글된다.
 *
 * 배선: 없음 (보드만 사용)
 *
 * 타이머 계산
 *   TIM2 클럭 64MHz (APB1 32MHz, 분주비 2 -> x2),  PSC=63 -> 1MHz (1us),  ARR=65535
 *   f_update = 64MHz / (64 x 65536) = 15.26Hz  (주기 65.536ms)
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler() 가 SR.UIF 를 지움
 *     -> HAL_TIM_PeriodElapsedCallback() (이 파일) : gOvf++ 한 줄만 수행
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
#define TEST_WINDOW_US   1000000UL   /* 경쟁 조건 관찰 구간 = 1초 */
/* Micros_Naive() 의 두 읽기 사이에 넣는 NOP 개수.
 * 0 이면 창이 수 사이클이라 back-jump 가 드물다(수 초에 한 번). 50 정도로 키우면 매초 여러 번 보인다.
 * "두 읽기 사이가 길어질수록 더 자주 틀린다"는 것을 확인하는 실험용 값이다. */
#define NAIVE_GAP_NOPS   0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint32_t gOvf;   /* TIM2 오버플로 횟수 (ISR 에서만 증가) = 시간의 상위 16비트 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t Micros_Naive(void);
static uint32_t Micros_Safe(void);
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

/*
 * (1) 순진한 읽기 — 경쟁 조건이 있다
 *   ① hi = gOvf 를 읽은 직후 ② CNT 를 읽기 전에 오버플로가 나면
 *   ISR 이 gOvf 를 올리지만 hi 는 이미 옛 값이고, lo(CNT)는 0 근처의 새 값이다.
 *   -> 결과가 실제보다 65536us 작아진다 (시간이 거꾸로 감).
 */
static uint32_t Micros_Naive(void)
{
	uint32_t hi = gOvf;          /* ① 상위 16비트 */
	for (int i = 0; i < NAIVE_GAP_NOPS; i++) __NOP();   /* 실험용: ①과 ② 사이 간격 늘리기 */
	uint32_t lo = TIM2->CNT;     /* ② 하위 16비트 (TIM2->CNT 직접 읽기) */
	return (hi << 16) | lo;
}

/*
 * (2) 안전한 읽기
 *   - 인터럽트를 잠시 막아 gOvf 와 CNT 를 같은 순간의 값으로 묶는다.
 *   - 막혀 있는 동안 오버플로가 나면 ISR 은 못 돌지만 하드웨어가 SR.UIF 를 세운다.
 *     UIF=1 이고 lo 가 작으면(=넘어간 뒤에 읽음) 아직 세지 못한 1회를 직접 더한다.
 *     lo 가 크면(0xFFFF 근처) 넘어가기 전에 읽은 값이므로 더하지 않는다.
 *   - PRIMASK 를 저장/복원하므로 ISR 안이나 이미 인터럽트가 막힌 곳에서 불러도 안전하다.
 */
static uint32_t Micros_Safe(void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	uint32_t hi = gOvf;
	uint32_t lo = TIM2->CNT;
	if ((TIM2->SR & TIM_SR_UIF) && (lo < 0x8000UL))
	{
		hi++;                    /* 아직 ISR 이 반영하지 못한 오버플로 */
	}
	__set_PRIMASK(primask);
	return (hi << 16) | lo;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t sec = 0;
  uint32_t printfUs = 0;     /* 직전 1초 보고 printf 한 줄의 실행 시간 */
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
  printf("\nTIM2 overflow counter start (1us tick, 16bit CNT + overflow count = 32bit us)\n");

  /* HAL_TIM_Base_Init() 의 EGR.UG 로 UIF 가 남아 있을 수 있다 -> 시작 직후 gOvf 가 1 이 되지 않도록 지운다 */
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);     /* TIM2->SR = ~TIM_SR_UIF */
  __HAL_TIM_SET_COUNTER(&htim2, 0);                  /* TIM2->CNT = 0 */
  gOvf = 0;

  /* 업데이트 인터럽트로 시작: DIER.UIE=1, CR1.CEN=1 */
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) Error_Handler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ---- 1) 1초 동안 두 읽기 함수를 번갈아 호출하며 "거꾸로 간" 횟수를 센다 ---- */
    uint32_t naiveBack = 0, safeBack = 0;
    uint32_t prevN = Micros_Naive();
    uint32_t prevS = Micros_Safe();
    uint32_t tEnd  = prevS + TEST_WINDOW_US;

    /* (int32_t)(a - b) < 0 : 32비트 래핑(71.6분)을 넘어도 올바른 비교 */
    while ((int32_t)(Micros_Safe() - tEnd) < 0)
    {
      uint32_t n = Micros_Naive();
      if ((int32_t)(n - prevN) < 0) naiveBack++;
      prevN = n;

      uint32_t s = Micros_Safe();
      if ((int32_t)(s - prevS) < 0) safeBack++;
      prevS = s;
    }

    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);    /* GPIOA->ODR ^= (1<<5) */
    sec++;

    /* ---- 2) HAL_Delay(10) 의 실제 길이를 us 시계로 잰다 ---- */
    uint32_t d0 = Micros_Safe();
    HAL_Delay(10);
    uint32_t delayUs = Micros_Safe() - d0;

    /* ---- 3) 보고. 이 printf 자체의 시간은 다음 줄에 표시된다 ---- */
    uint32_t p0 = Micros_Safe();
    printf("[%3lu s] t=%10lu us ovf=%5lu | back-jumps naive=%lu safe=%lu | HAL_Delay(10)=%lu us | prev printf=%lu us\n",
           (unsigned long)sec, (unsigned long)p0, (unsigned long)gOvf,
           (unsigned long)naiveBack, (unsigned long)safeBack,
           (unsigned long)delayUs, (unsigned long)printfUs);
    printfUs = Micros_Safe() - p0;
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

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 63;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */
  /* TIM2: PSC=63 -> 1MHz(1us), ARR=0xFFFF 자유 계수 -> 65.536ms 마다 업데이트(오버플로) 이벤트 */
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
/* HAL 콜백 (ISR 문맥, HAL_TIM_IRQHandler 가 SR.UIF 를 지운 뒤 호출)              */
/* ------------------------------------------------------------------------- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    gOvf++;          /* 오버플로 1회 = 65536us. 콜백은 이 한 줄로 끝낸다 */
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

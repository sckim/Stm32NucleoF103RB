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
 * 11_NonBlocking_StateMachine_c  —  [계층: HAL]  HAL_Delay 없이 상태 머신(FSM)으로 만든 보행자 신호등
 *
 * 왜 상태 머신인가
 *   HAL_Delay() 로 순서대로 쓰면 그 시간 동안 버튼 입력·통신 등 다른 일을 전혀 못 한다.
 *   "지금 상태"와 "상태가 시작된 시각"만 기억하고, 루프를 계속 돌리며 (a) 시간 경과 (b) 버튼 이벤트를 확인해 상태를 바꾸면
 *   여러 일이 겹쳐도 반응성이 유지된다. (delay 탈출)
 *
 * 신호등 시나리오 (LED 3개)
 *   G(녹색) 5초 -> Y(황색) 1초 -> R(적색) 3초 -> G ...
 *   보행자 버튼(B1): 녹색 상태에서 최소 2초가 지난 뒤 누르면 남은 녹색을 건너뛰고 바로 황색으로 (녹색 최소 보장 시간).
 *   황색/적색 중 누른 경우는 요청만 기억했다가 다음 녹색이 최소 시간을 채운 뒤 즉시 황색으로 전환.
 *
 * 핀 (Nucleo-F103RB) : 녹색 = LD2(PA5), 황색 = PB0 ('A3'), 적색 = PB1 (Morpho CN10). 외부 LED 는 330Ω 저항과 직렬로 GND 로.
 *
 * 구현 요점
 *   - 상태 = enum, 전환 조건 = 시간(HAL_GetTick 차이, 오버플로 안전) 또는 이벤트
 *   - 진입 동작(entry action)은 전환 함수 한 곳에서만 수행 -> 상태가 바뀔 때 LED/로그가 항상 일관됨
 *   - 버튼은 5ms 마다 샘플링하는 "연속 N회 일치" 디바운스, 눌림 "에지"에서만 이벤트 발생
 *   - 상태 전환 로그를 UART(115200)로 출력 (시간 표시). 로그 출력 중에도 폴링 주기가 크게 흔들리지 않도록 짧은 문자열만 사용
 *
 * 인터럽트를 사용하지 않는다 (SysTick 이 HAL_GetTick 을 위해 돌고 있을 뿐).
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
typedef enum { ST_GREEN, ST_YELLOW, ST_RED } state_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define T_GREEN_MS      5000U
#define T_GREEN_MIN_MS  2000U
#define T_YELLOW_MS     1000U
#define T_RED_MS        3000U
#define DEBOUNCE_N      4U          /* 5ms x 4 = 20ms 연속 일치 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static state_t state;
static uint32_t state_since;
static uint8_t ped_request;

static const char *const state_name[] = { "GREEN", "YELLOW", "RED" };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void set_leds(state_t s)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, s == ST_GREEN  ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_Y_GPIO_Port, LED_Y_Pin, s == ST_YELLOW ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, s == ST_RED    ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 상태 전환의 유일한 통로: 진입 동작(LED, 시각, 로그)을 한 곳에 모은다 */
static void enter_state(state_t next, const char *reason)
{
    state = next;
    state_since = HAL_GetTick();
    set_leds(next);

    char m[64];
    int n = snprintf(m, sizeof m, "[%6lu ms] -> %-6s (%s)\r\n", (unsigned long)state_since, state_name[next], reason);
    HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 50);
}

/* 5ms 마다 호출: 연속 N회 같은 값이면 확정, 눌림 에지에서 1 반환 */
static int button_pressed_edge(void)
{
    static uint8_t cnt, stable = 1, last_raw = 1;
    uint8_t raw = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);   /* 1 = 뗌, 0 = 눌림 */
    if (raw == last_raw) { if (cnt < DEBOUNCE_N) cnt++; } else { cnt = 0; last_raw = raw; }

    if (cnt >= DEBOUNCE_N && raw != stable)
    {
        stable = raw;
        if (stable == 0) return 1;                     /* 눌림 에지 */
    }
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
  /* USER CODE BEGIN 2 */
  HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n[FSM traffic light] B1 = pedestrian request\r\n", 47, 100);
  enter_state(ST_GREEN, "boot");

  uint32_t t_key = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();
    uint32_t in_state = now - state_since;         /* 부호 없는 뺄셈: 49일 래핑에도 안전 */

    /* ---- 이벤트 수집 (5ms 주기) ---- */
    if (now - t_key >= 5)
    {
      t_key += 5;
      if (button_pressed_edge()) ped_request = 1;
    }

    /* ---- 상태별 전환 규칙 ---- */
    switch (state)
    {
    case ST_GREEN:
      if (ped_request && in_state >= T_GREEN_MIN_MS) { ped_request = 0; enter_state(ST_YELLOW, "pedestrian request"); }
      else if (in_state >= T_GREEN_MS)               {                  enter_state(ST_YELLOW, "timeout"); }
      break;
    case ST_YELLOW:
      if (in_state >= T_YELLOW_MS) enter_state(ST_RED, "timeout");
      break;
    case ST_RED:
      if (in_state >= T_RED_MS) enter_state(ST_GREEN, "timeout");
      break;
    }
    /* 이 자리에 다른 논블로킹 작업을 자유롭게 추가할 수 있다 (센서 읽기, 통신 처리 등) */
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
  HAL_GPIO_WritePin(GPIOB, LED_Y_Pin|LED_R_Pin, GPIO_PIN_RESET);

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

  /*Configure GPIO pins : LED_Y_Pin LED_R_Pin */
  GPIO_InitStruct.Pin = LED_Y_Pin|LED_R_Pin;
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

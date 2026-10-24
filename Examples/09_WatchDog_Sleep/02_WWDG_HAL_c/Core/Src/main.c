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
 * 02_WWDG_HAL_c  —  [계층: HAL]  윈도우 워치독(WWDG) — "너무 늦게"뿐 아니라 "너무 빨리" 리프레시해도 리셋
 *
 * IWDG 와의 차이 (01_WatchdogTimer 는 이름과 달리 WWDG 기본 사용 예제)
 *   IWDG : 독립 저속 클럭(LSI 40kHz), 타임아웃 전에만 리프레시하면 됨(늦는 것만 감시).
 *   WWDG : APB1 클럭 기반 7비트 다운카운터(T[6:0]). 카운터가 "윈도우 값 이하"일 때만 리프레시 허용.
 *          윈도우 밖(너무 이름) 리프레시, 0x3F 이하로 내려감(너무 늦음) 모두 시스템 리셋.
 *          -> 프로그램이 "정해진 타이밍에" 실행되는지까지 감시한다.
 *
 * 타이밍 계산 (PCLK1 = 32MHz)
 *   t_tick = 4096 x prescaler(8) / 32MHz = 1.024 ms
 *   카운터 시작 0x7F, 윈도우 0x50, 리셋 임계 0x3F
 *     리프레시 허용 구간 : 카운터 <= 0x50  ->  (0x7F-0x50) x 1.024 = 약 48 ms 이후 ~
 *     리셋(타임아웃)     : 카운터 < 0x40   ->  (0x7F-0x3F) x 1.024 = 약 65.5 ms
 *   => 리프레시는 약 48ms ~ 65ms 사이에만 해야 한다. 이 예제는 55ms 마다 리프레시.
 *
 * 하드웨어 대응
 *   WWDG->CR : WDGA(bit7)=1 활성화(한 번 켜면 끌 수 없음), T[6:0] 카운터
 *   WWDG->CFR: W[6:0] 윈도우, WDGTB[8:7] 프리스케일러, EWI(bit9) 조기 경고 인터럽트
 *   WWDG->SR : EWIF(bit0)
 *   RCC->CSR : WWDGRSTF(bit30) 리셋 원인 플래그, RMVF(bit24) 플래그 지우기
 *
 * 시험 방법
 *   - 기본: 55ms 주기 리프레시로 LD2 가 정상 점멸.
 *   - B1(PC13) 누름: 프로그램 "멈춤" 흉내(리프레시 중단) -> 약 65ms 뒤 리셋 -> 부팅 메시지에 WWDG 리셋 표시
 *   - EARLY_REFRESH_TEST 를 1 로 바꾸면 20ms 주기(너무 이름)로 리프레시 -> 계속 리셋되는 것을 확인
 *
 * ISR vs 콜백
 *   WWDG_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_WWDG_IRQHandler()
 *     -> HAL_WWDG_EarlyWakeupCallback() (이 파일) : 카운터가 0x40 이 되는 순간, 리셋 직전 "마지막 기회"
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
#define EARLY_REFRESH_TEST 0     /* 1: 윈도우 밖(너무 이른) 리프레시 시험 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

WWDG_HandleTypeDef hwwdg;

/* USER CODE BEGIN PV */
static volatile uint32_t ewi_count;   /* 조기 경고가 발생한 횟수 (리셋 직전이므로 보통 1 이후 리셋) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_WWDG_Init(void);
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
  char msg[80];
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  __HAL_DBGMCU_FREEZE_WWDG();                          /* DBGMCU->CR.DBG_WWDG_STOP = 1: 디버거 중단점에서 카운터 정지 */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_WWDG_Init();
  /* USER CODE BEGIN 2 */
  /* 리셋 원인 확인 (RCC->CSR).
   * CubeMX 구조에서는 MX_WWDG_Init() 가 USER CODE 2 보다 먼저 호출되어 워치독은 이미 동작 중이다.
   * 아래 출력은 수 ms 이내라 윈도우(약 48ms) 이전이고, 첫 리프레시는 while 루프의 HAL_Delay(55) 뒤에 일어난다. */
  const char *cause = __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? "WWDG (window watchdog)"
                    : __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)  ? "POWER-ON"
                    : "other (pin/software/IWDG)";
  int n = snprintf(msg, sizeof msg, "\r\n[boot] reset cause: %s\r\n", cause);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
  __HAL_RCC_CLEAR_RESET_FLAGS();                       /* CSR.RMVF = 1 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if EARLY_REFRESH_TEST
    HAL_Delay(20);                                   /* 윈도우(약 48ms) 이전 -> 리프레시 즉시 리셋 */
#else
    HAL_Delay(55);                                   /* 48ms < 55ms < 65ms : 허용 구간 */
#endif
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* GPIOC->IDR bit13 (B1: 누르면 Low) */
    {
      while (1) {}                                   /* 프로그램 멈춤 흉내: 리프레시 없이 대기 -> WWDG 리셋 */
    }

    if (HAL_WWDG_Refresh(&hwwdg) != HAL_OK) Error_Handler();   /* WWDG->CR.T = 0x7F */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);      /* 55ms 마다 토글: 육안으로는 빠른 점멸 (GPIOA->ODR bit5) */
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
  /* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용. 클럭/핀 설정은 stm32f1xx_hal_msp.c 의 HAL_UART_MspInit 에 있다. */
  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief WWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_WWDG_Init(void)
{

  /* USER CODE BEGIN WWDG_Init 0 */

  /* USER CODE END WWDG_Init 0 */

  /* USER CODE BEGIN WWDG_Init 1 */

  /* USER CODE END WWDG_Init 1 */
  hwwdg.Instance = WWDG;
  hwwdg.Init.Prescaler = WWDG_PRESCALER_8;
  hwwdg.Init.Window = 80;
  hwwdg.Init.Counter = 127;
  hwwdg.Init.EWIMode = WWDG_EWI_ENABLE;
  if (HAL_WWDG_Init(&hwwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN WWDG_Init 2 */
  /* [레지스터] Prescaler 8 -> WWDG->CFR.WDGTB = 11, Window 0x50 -> CFR.W, Counter 0x7F -> CR.T,
   * EWI -> CFR.EWI(bit9) = 1, HAL_WWDG_Init 가 CR.WDGA(bit7) = 1 로 활성화 (한 번 켜면 끌 수 없음) */
  /* USER CODE END WWDG_Init 2 */

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
/* HAL 콜백 (ISR 아님, HAL_WWDG_IRQHandler 가 호출)                            */
/* ------------------------------------------------------------------------- */
void HAL_WWDG_EarlyWakeupCallback(WWDG_HandleTypeDef *hwwdg_)
{
  (void)hwwdg_;
  ewi_count++;
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);  /* 리셋 직전 LED 를 켜 두어 "마지막 기회"가 실행됐음을 표시 */
  /* 여기서 리프레시하면 리셋을 막을 수 있으나, 그러면 워치독의 의미가 없어진다.
     실제 제품에서는 중요한 상태를 백업 레지스터/플래시에 저장하는 용도로 쓴다 (약 1ms 남음). */
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

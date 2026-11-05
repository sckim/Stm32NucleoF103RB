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
 * 05_Standby_RTC_Wakeup_c  —  [계층: HAL]  Standby 모드 + RTC 알람 웨이크업 + 백업 레지스터 유지
 *
 * 동작
 *   부팅 -> (Standby 복귀인지 판별) -> BKP 부팅 카운터 +1 -> LD2 1초 점등 -> RTC 알람 = 지금 + 10초 설정
 *   -> Standby 진입.  10초 뒤 RTC 알람이 MCU 를 깨우면 "리셋과 동일하게" main 부터 다시 시작한다.
 *   UART(115200)에는 리셋 원인(POWER-ON / STANDBY-WAKEUP)과 BKP 부팅 카운터가 출력된다.
 *   WKUP 핀(PA0)에 3.3V 를 인가해도 깨어난다.
 *
 * Standby vs Stop vs Sleep (RM0008 5장)
 *   Sleep   : CPU 클럭만 정지, 모든 상태 유지, 웨이크업 즉시            (03_Sleep_Mode)
 *   Stop    : 고속 클럭 정지, SRAM/레지스터 유지, 웨이크업 후 이어서 실행  (04_Stop_Mode_EXTI)
 *   Standby : 1.8V 도메인 전원 차단. SRAM·레지스터 내용 "소실", 웨이크업 = 리셋.  가장 낮은 전류(약 2uA)
 *             유지되는 것: 백업 도메인(RTC, BKP 백업 레지스터 DR1~DR10) 과 Standby 회로
 *   -> Standby 에서 상태를 이어가려면 BKP 레지스터/백업 SRAM 에 저장해 두어야 한다.
 *
 * 핵심 레지스터/비트
 *   PWR->CR.PDDS=1 + SCB->SCR.SLEEPDEEP=1 + WFI  (HAL_PWR_EnterSTANDBYMode)
 *   PWR->CSR.SBF : "Standby 에서 깨어났음" 플래그 (CR.CSBF 에 1 을 써서 지움)
 *   PWR->CSR.EWUP: WKUP 핀(PA0) 웨이크업 허용
 *   RTC->ALRH/ALRL : 알람 값,  RTC->CRH.ALRIE : 알람 인터럽트 허용,  RTC->CRL.ALRF : 알람 플래그
 *   BKP->DR1~DR10 : 백업 레지스터 (PWR->CR.DBP = 1 로 쓰기 허용 필요). HAL_RTCEx_BKUPRead/Write
 *   RTC 클럭 소스: LSE(32.768kHz 크리스털)를 먼저 시도하고, 없으면 LSI(약 40kHz)로 대체
 *
 * 주의: Standby 중에는 SWD 디버거 연결이 끊긴다. 전류를 잴 때는 IDD 점퍼(JP6)로 측정.
 *       RTC 알람 웨이크업 후 HAL_RTC 를 다시 초기화하지만, 백업 도메인 리셋(BDRST)은 절대 하지 않는다 (BKP 유지).
 *
 * ISR: RTC 알람은 Standby 웨이크업(리셋) 용도라 별도 ISR/콜백이 필요 없다.
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
#define BKP_MAGIC_REG   RTC_BKP_DR1      /* RTC/백업 도메인이 이미 설정됐는지 표시 */
#define BKP_MAGIC       0x32F2U
#define BKP_COUNT_REG   RTC_BKP_DR2      /* 부팅 횟수 */
#define ALARM_SECONDS   10U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
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
  char msg[96];
  int from_standby;
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
  MX_RTC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* PWR 클럭(RCC->APB1ENR.PWREN)은 HAL_MspInit, BKP 클럭(BKPEN)과 PWR->CR.DBP = 1 은 HAL_RTC_MspInit 에서 이미 켜졌다 */
  from_standby = __HAL_PWR_GET_FLAG(PWR_FLAG_SB);       /* PWR->CSR.SBF */
  if (from_standby) __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  /* PWR->CR.CSBF = 1 */

  uint32_t cnt = HAL_RTCEx_BKUPRead(&hrtc, BKP_COUNT_REG) + 1;
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_COUNT_REG, cnt);

  int n = snprintf(msg, sizeof msg, "\r\n[boot #%lu] reset cause: %s\r\n", (unsigned long)cnt,
                   from_standby ? "STANDBY-WAKEUP (RTC alarm or WKUP pin)" : "POWER-ON / RESET");
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /* 알람 = 현재 RTC 시각 + 10초 (시:분:초 로 환산, 24시간 래핑 처리) */
  RTC_TimeTypeDef t;
  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  uint32_t sec = (t.Hours * 3600U + t.Minutes * 60U + t.Seconds + ALARM_SECONDS) % 86400U;
  RTC_AlarmTypeDef a = {0};
  a.Alarm = RTC_ALARM_A;                             /* F1 에는 알람이 하나 (필드는 시분초만 사용) */
  a.AlarmTime.Hours   = (uint8_t)(sec / 3600U);
  a.AlarmTime.Minutes = (uint8_t)((sec / 60U) % 60U);
  a.AlarmTime.Seconds = (uint8_t)(sec % 60U);
  if (HAL_RTC_SetAlarm_IT(&hrtc, &a, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();   /* RTC->ALR, CRH.ALRIE */

  n = snprintf(msg, sizeof msg, "alarm set for %02u:%02u:%02u, entering STANDBY...\r\n",
               a.AlarmTime.Hours, a.AlarmTime.Minutes, a.AlarmTime.Seconds);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

  __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF); /* 진입 전 오래된 알람 플래그 제거 */
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);                 /* PWR->CR.CWUF: 이전 웨이크업 플래그 제거 */
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);          /* PWR->CSR.EWUP = 1 : PA0 웨이크업 허용 */

  HAL_PWR_EnterSTANDBYMode();                        /* 여기서 정지. 깨어나면 main 처음부터 다시 시작 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Standby 진입 후에는 도달하지 않음 (웨이크업 = 리셋) */
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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
  /* [레지스터] AsynchPrediv = RTC_AUTO_1_SECOND: 선택된 RTC 클럭에서 1초 분주비를 자동 계산해 RTC->PRLH/PRLL 에 기록.
   * 백업 도메인 리셋(RCC->BDCR.BDRST)은 절대 하지 않는다 (BKP/RTC 값 유지). 클럭 소스 선택은 HAL_RTC_MspInit 참고. */
  /* USER CODE END RTC_Init 2 */

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

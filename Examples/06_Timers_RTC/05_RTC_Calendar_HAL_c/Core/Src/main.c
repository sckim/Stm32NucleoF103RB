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
 * 05_RTC_Calendar_HAL_c  —  [계층: HAL(RTC 초기화/1초 인터럽트) + 레지스터(카운터 접근)]  RTC 달력 시계
 *
 * STM32F1 RTC 의 특징
 *   F1 의 RTC 는 "달력" 하드웨어가 없다. 32비트 카운터(RTC->CNTH:CNTL)가 1초마다 1 증가할 뿐이다.
 *   -> 이 카운터를 Unix 시간(1970-01-01 00:00:00 부터 경과한 초)으로 쓰고, 날짜/시각은 소프트웨어로 계산한다.
 *   (이후 시리즈의 RTC 는 BCD 달력 하드웨어가 있지만 F1 은 없다. HAL 의 F1 달력 API 도 이 방식을 흉내 낸 것이다.)
 *
 * 동작
 *   - 전원 최초 인가 시 컴파일 시각(__DATE__ / __TIME__)으로 카운터를 설정하고 BKP DR1 에 "설정됨" 표시.
 *   - RTC 1초 인터럽트마다 카운터를 읽어 "YYYY-MM-DD HH:MM:SS (요일)" 을 UART(115200)로 출력, LD2 토글.
 *   - NRST(리셋) 버튼을 눌러도 시각이 이어진다 (백업 도메인이 유지되기 때문). 전원을 완전히 끄면 VBAT 이 없어 초기화된다.
 *   - B1(PC13) 을 누르면 컴파일 시각으로 다시 설정.
 *
 * 핵심 레지스터/비트
 *   RTC->CNTH:CNTL : 32비트 카운터.  읽기: CNTH-CNTL-CNTH 를 읽어 상위가 바뀌었으면 재시도(자리올림 경합 방지)
 *   쓰기 절차: RTC->CRL.RTOFF=1 대기 -> CRL.CNF=1(설정 모드) -> CNTH/CNTL 기록 -> CNF=0 -> RTOFF=1 대기
 *   RTC->CRH.SECIE : 1초 인터럽트 허용,  RTC->CRL.SECF : 1초 플래그
 *   PWR->CR.DBP=1 : 백업 도메인 쓰기 허용 (RTC/BKP 레지스터 접근에 필요)
 *   RCC->BDCR : LSEON/LSERDY, RTCSEL[9:8], RTCEN[15]
 *
 * ISR vs 콜백
 *   RTC_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_RTCEx_RTCIRQHandler()
 *     -> HAL_RTCEx_RTCEventCallback() (이 파일) : 1초 이벤트. 플래그만 세우고 출력은 main 에서.
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BKP_MAGIC_REG RTC_BKP_DR1
#define BKP_MAGIC     0x32F2U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint8_t second_flag;
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
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

/* ---- RTC 32비트 카운터 접근 (레지스터) ---- */
static uint32_t rtc_get_epoch(void)
{
    uint16_t hi, lo;
    do {
        hi = (uint16_t)RTC->CNTH;
        lo = (uint16_t)RTC->CNTL;
    } while (hi != (uint16_t)RTC->CNTH);              /* 읽는 사이 자리올림이 있었으면 재시도 */
    return ((uint32_t)hi << 16) | lo;
}

static void rtc_set_epoch(uint32_t v)
{
    while (!(RTC->CRL & RTC_CRL_RTOFF)) {}            /* 이전 쓰기 완료 대기 */
    RTC->CRL |= RTC_CRL_CNF;                          /* 설정 모드 진입 */
    RTC->CNTH = v >> 16;
    RTC->CNTL = v & 0xFFFFU;
    RTC->CRL &= ~RTC_CRL_CNF;                         /* 설정 모드 탈출 (이때 실제 기록됨) */
    while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
}

/* ---- Unix 시간 <-> 날짜 변환 (그레고리력, 윤년 포함) ---- */
static void epoch_to_datetime(uint32_t e, int *y, int *mo, int *d, int *h, int *mi, int *s, int *wd)
{
    uint32_t days = e / 86400U, rem = e % 86400U;
    *h = (int)(rem / 3600U);  *mi = (int)((rem / 60U) % 60U);  *s = (int)(rem % 60U);
    *wd = (int)((days + 4U) % 7U);                    /* 1970-01-01 은 목요일(4). 0=일 */

    /* civil_from_days (H. Hinnant): 1970-01-01 기준 일수 -> 년/월/일 */
    int32_t z = (int32_t)days + 719468;
    int32_t era = z / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t yy = yoe + era * 400;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*mo <= 2));
}

static uint32_t datetime_to_epoch(int y, int mo, int d, int h, int mi, int s)
{
    y -= (mo <= 2);
    int32_t era = y / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int32_t days = era * 146097 + doe - 719468;
    return (uint32_t)days * 86400U + (uint32_t)(h * 3600 + mi * 60 + s);
}

/* __DATE__ = "Sep 21 2026", __TIME__ = "12:34:56" 를 파싱해 Unix 시간으로 */
static uint32_t compile_time_epoch(void)
{
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *dt = __DATE__;
    int mo = 1;
    for (int i = 0; i < 12; i++) if (strncmp(dt, &mon[i * 3], 3) == 0) { mo = i + 1; break; }
    int d = (dt[4] == ' ') ? (dt[5] - '0') : ((dt[4] - '0') * 10 + (dt[5] - '0'));
    int y = atoi(&dt[7]);
    const char *tm = __TIME__;
    int h = (tm[0] - '0') * 10 + (tm[1] - '0');
    int mi = (tm[3] - '0') * 10 + (tm[4] - '0');
    int s = (tm[6] - '0') * 10 + (tm[7] - '0');
    return datetime_to_epoch(y, mo, d, h, mi, s);
}

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
  /* 백업 도메인 접근(PWR->CR.DBP)/BKP 클럭은 HAL_RTC_MspInit (stm32f1xx_hal_msp.c) 에서 이미 허용했다 */
  if (HAL_RTCEx_BKUPRead(&hrtc, BKP_MAGIC_REG) != BKP_MAGIC)     /* 최초 전원 인가 */
  {
    rtc_set_epoch(compile_time_epoch());
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_MAGIC_REG, BKP_MAGIC);
    print("\r\nRTC set from compile time\r\n");
  }
  else
  {
    print("\r\nRTC already running (backup domain kept)\r\n");
  }

  HAL_RTCEx_SetSecond_IT(&hrtc);                     /* RTC->CRH.SECIE = 1 */

  static const char *const wdn[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (second_flag)
    {
      second_flag = 0;
      int y, mo, d, h, mi, s, wd;
      epoch_to_datetime(rtc_get_epoch(), &y, &mo, &d, &h, &mi, &s, &wd);
      int n = snprintf(msg, sizeof msg, "%04d-%02d-%02d %02d:%02d:%02d (%s)\r\n", y, mo, d, h, mi, s, wdn[wd]);
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }

    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* B1 : 컴파일 시각으로 재설정 (GPIOC->IDR bit13) */
    {
      HAL_Delay(30);
      rtc_set_epoch(compile_time_epoch());
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
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

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

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
/* HAL 콜백 (ISR 아님): RTC 1초 이벤트 (SECF)                                    */
/* ------------------------------------------------------------------------- */
void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc_)
{
  (void)hrtc_;
  second_flag = 1;
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

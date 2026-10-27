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
 * 03_ADC_Multi_c  —  [계층: HAL]  DMA 없이 여러 채널 읽기: 스캔 모드 + 불연속(discontinuous) 모드 + 폴링
 *
 * 문제: ADC1 레귤러 그룹의 결과 레지스터(DR)는 하나뿐이다. 스캔 모드로 4채널을 한 번에 변환하면
 *       채널마다 DR 이 덮어써지고, EOC 는 시퀀스 끝에서만 서므로 CPU 가 중간 값을 놓친다.
 *       (그래서 보통 DMA 를 쓴다 -> 04_ADC_DMA_TimTrigger_c)
 * 해법: 불연속 모드(DISCNUM = 1)를 켜면 트리거 1번에 시퀀스의 "다음 1채널"만 변환한다.
 *       Start -> 변환 완료 대기 -> DR 읽기 를 채널 수만큼 반복하면 순서대로 모든 채널을 얻는다.
 *       마지막 랭크까지 변환하면 다음 트리거는 다시 랭크 1부터 시작한다 (RM0008 11.3.10).
 *
 * 변환 시퀀스 (SQR3)
 *   랭크1 CH0  (PA0, Arduino A0)   샘플 55.5 사이클
 *   랭크2 CH1  (PA1, Arduino A1)   샘플 55.5 사이클
 *   랭크3 CH4  (PA4, Arduino A2)   샘플 55.5 사이클
 *   랭크4 CH17 (Vrefint, 내부)     샘플 239.5 사이클 (내부 소스 임피던스가 높음, 최소 17.1us)
 *
 * 동작
 *   - 4채널 스캔을 N_AVG 회 반복해 채널별 평균
 *   - Vrefint 로 실제 VDDA 를 역산하고 (06_ADC_Vrefint_VDDA_c 참고) 외부 채널을 mV 로 환산
 *   - 1초마다 UART(115200) 출력
 *
 * 하드웨어 대응 (RM0008)
 *   ADC1->CR1.SCAN = 1                 : 스캔 모드 (SQR 시퀀스를 순서대로)
 *   ADC1->CR1.DISCEN = 1, DISCNUM = 000 : 불연속 모드, 트리거당 1채널
 *   ADC1->CR2: EXTSEL = 111 (SWSTART), EXTTRIG = 1, CONT = 0, DMA = 0
 *   ADC1->SQR1.L = 3 (변환 4개), SQR3 = {CH0, CH1, CH4, CH17}
 *   ADC1->SMPR2 (CH0/1/4), SMPR1 (CH17), ADC1->CR2.TSVREFE (HAL 이 CH17 설정 시 자동으로 켬)
 *   ADC 클럭 = PCLK2(64MHz)/6 = 10.67MHz -> 외부 채널 (55.5+12.5)=68 사이클 = 6.4us, Vrefint 252 사이클 = 23.6us
 *
 * HAL 주의점: 스캔 모드(SQR1.L >= 1)에서는 EOC 가 채널마다 서지 않으므로 HAL_ADC_PollForConversion() 이
 *   EOC 를 기다리지 않고 "최대 변환 시간"만큼 소프트웨어 지연 후 반환한다 (stm32f1xx_hal_adc.c 주석 참고).
 *
 * 인터럽트를 사용하지 않는다 (폴링 변환). ISR 은 SysTick_Handler(stm32f1xx_it.c) 뿐이다.
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
#define N_CH       4U
#define N_AVG      8U
#define VREFINT_MV 1200UL          /* 데이터시트 전형값 (개체 편차 1.16~1.24V) */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 시퀀스 1회(랭크1~4)를 채널 하나씩 변환해 out[] 에 저장 */
static void adc_scan_once(uint16_t out[N_CH])
{
    for (uint32_t r = 0; r < N_CH; r++)
    {
        HAL_ADC_Start(&hadc1);                                    /* CR2.SWSTART -> 다음 랭크 1채널 변환 */
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) Error_Handler();   /* 스캔 모드: 변환 시간만큼 대기 */
        out[r] = (uint16_t)HAL_ADC_GetValue(&hadc1);              /* DR 읽기 (다음 변환 전에 읽어야 함) */
    }
    /* HAL_ADC_Stop() 은 호출하지 않는다: ADON 을 유지한 채 다음 Start 가 랭크1부터 이어진다 */
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char msg[128];
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
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();   /* ADC1->CR2.CAL */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t sum[N_CH] = {0};
    uint16_t raw[N_CH];
    for (uint32_t i = 0; i < N_AVG; i++)
    {
      adc_scan_once(raw);
      for (uint32_t c = 0; c < N_CH; c++) sum[c] += raw[c];
    }
    uint32_t avg[N_CH];
    for (uint32_t c = 0; c < N_CH; c++) avg[c] = sum[c] / N_AVG;

    /* VDDA = Vrefint x 4095 / raw_vref,  V_ch = raw x VDDA / 4095 */
    uint32_t vdda_mv = (avg[3] != 0) ? VREFINT_MV * 4095UL / avg[3] : 0;
    int n = snprintf(msg, sizeof msg,
                     "A0=%4lu(%4lumV) A1=%4lu(%4lumV) A2=%4lu(%4lumV) | Vref=%4lu -> VDDA=%4lumV\r\n",
                     (unsigned long)avg[0], (unsigned long)(avg[0] * vdda_mv / 4095UL),
                     (unsigned long)avg[1], (unsigned long)(avg[1] * vdda_mv / 4095UL),
                     (unsigned long)avg[2], (unsigned long)(avg[2] * vdda_mv / 4095UL),
                     (unsigned long)avg[3], (unsigned long)vdda_mv);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    HAL_Delay(1000);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = ENABLE;
  hadc1.Init.NbrOfDiscConversion = 1;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* 변환 시퀀스 (SQR3.SQ1~SQ4) / 샘플 시간 (SMPR2: CH0/1/4, SMPR1: CH17)
   *   랭크1 CH0 (PA0) 55.5, 랭크2 CH1 (PA1) 55.5, 랭크3 CH4 (PA4) 55.5, 랭크4 CH17 (Vrefint) 239.5 사이클
   * ADC1->CR1.SCAN = 1, CR1.DISCEN = 1, CR1.DISCNUM = 000 (트리거당 1채널), CR2.CONT = 0
   * ADC1->CR2.EXTSEL = 111 (SWSTART), ADC1->SQR1.L = 3 (변환 4개), CR2.ALIGN = 0
   * ADC 클럭 prescaler (RCC->CFGR.ADCPRE = 64MHz/6 = 10.67MHz) 는 SystemClock_Config() 에서 설정된다. */
  /* USER CODE END ADC1_Init 2 */

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

  /*Configure GPIO pins : PA0 PA1 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

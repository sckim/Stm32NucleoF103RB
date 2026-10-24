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
 * 04_ADC_AnalogWatchdog_HAL_c  —  [계층: HAL]  ADC 아날로그 워치독(AWD): 전압이 창(window)을 벗어나면 하드웨어가 인터럽트
 *
 * 개념: ADC 가 변환을 끝낼 때마다 "결과 > 상한 또는 결과 < 하한"인지 하드웨어가 비교하고, 벗어나면 플래그/인터럽트를 낸다.
 *       CPU 가 값을 계속 읽어 비교하지 않아도 되므로 과전압/저전압/과전류 보호에 적합하다.
 *
 * 동작
 *   - PA0(A0) 의 전압을 연속 변환(DMA 로 최신값을 변수에 자동 저장). 창: 1000 ~ 3000 (약 0.81V ~ 2.42V @3.3V)
 *   - 창을 벗어나면 AWD 인터럽트 -> 콜백이 LD2 를 켜고 플래그 설정. 창 안으로 돌아오면 main 이 LD2 를 끄고 AWD 를 다시 무장.
 *   - 200ms 마다 현재 값과 상태를 UART(115200)로 출력.  가변저항으로 PA0 를 0~3.3V 로 돌리며 확인.
 *
 * 하드웨어 대응 (RM0008 11.3.7 Analog watchdog)
 *   ADC1->HTR / LTR : 상한/하한 임계값(12비트)
 *   ADC1->CR1.AWDEN(bit23) : 정규 채널 AWD 허용,  CR1.AWDSGL(bit9) : 단일 채널만 감시,  CR1.AWDCH : 감시 채널,  CR1.AWDIE(bit6) : AWD 인터럽트
 *   ADC1->SR.AWD(bit0) : 창을 벗어났음 플래그
 *   *** 창을 벗어난 상태에서는 "매 변환마다" AWD 가 다시 발생해 인터럽트가 폭주한다(초당 수만 회).
 *       그래서 콜백에서 AWD 인터럽트를 끄고, main 이 창 안으로 돌아온 것을 확인한 뒤 다시 켠다. ***
 *   ADC 클럭 = 64MHz/6 = 10.67MHz, 샘플 239.5 사이클 -> 약 42kSPS
 *
 * ISR vs 콜백
 *   ADC1_2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_ADC_IRQHandler() -> HAL_ADC_LevelOutOfWindowCallback() (이 파일)
 *   (DMA 전송 완료 인터럽트는 NVIC 에서 허용하지 않아 ISR 부하가 없다)
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
#define AWD_HIGH 3000U
#define AWD_LOW  1000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint16_t adc_val;          /* DMA 가 계속 갱신하는 최신 변환값 */
static volatile uint8_t  awd_flag;
static volatile uint32_t awd_events;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADCEx_Calibration_Start(&hadc1);                                                    /* ADC1->CR2.CAL */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1) != HAL_OK) Error_Handler();   /* 1워드 원형 DMA: 항상 최신값 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(200);
    uint16_t v = adc_val;
    int in_window = (v >= AWD_LOW && v <= AWD_HIGH);

    if (in_window && awd_flag)                       /* 창 안으로 복귀 -> 재무장 */
    {
      awd_flag = 0;
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
      __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD);  /* ADC1->SR.AWD = 0 */
      __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_AWD);     /* CR1.AWDIE = 1 */
    }

    int n = snprintf(msg, sizeof msg, "ADC=%4u (%4lu mV)  window[%u..%u]  %s  events=%lu\r\n",
                     v, (unsigned long)v * 3300UL / 4095UL, AWD_LOW, AWD_HIGH,
                     awd_flag ? "OUT-OF-WINDOW" : "ok", (unsigned long)awd_events);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
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

  ADC_AnalogWDGConfTypeDef AnalogWDGConfig = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the analog watchdog
  */
  AnalogWDGConfig.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;
  AnalogWDGConfig.HighThreshold = 3000;
  AnalogWDGConfig.LowThreshold = 1000;
  AnalogWDGConfig.Channel = ADC_CHANNEL_0;
  AnalogWDGConfig.ITMode = ENABLE;
  if (HAL_ADC_AnalogWDGConfig(&hadc1, &AnalogWDGConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* ADC1->CR2.CONT = 1 (연속 변환), 샘플 239.5 사이클 (SMPR2), SQR3.SQ1 = CH0
   * 아날로그 워치독: ADC1->CR1.AWDEN = 1, AWDSGL = 1, AWDCH = 0 (CH0), AWDIE = 1 (HAL_ADC_AnalogWDGConfig)
   *                  ADC1->HTR = 3000 (상한), ADC1->LTR = 1000 (하한). 창 이탈 시 ADC1->SR.AWD(bit0) 가 선다.
   * ADC 클럭 prescaler (RCC->CFGR.ADCPRE, 64/6 = 10.67MHz) 는 SystemClock_Config() 에서 설정된다. */
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

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
/* HAL 콜백 (ISR 문맥): 변환값이 창을 벗어났을 때 (SR.AWD)                        */
/* ------------------------------------------------------------------------- */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    awd_flag = 1;
    awd_events++;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);   /* 즉시 경고 표시 */
    __HAL_ADC_DISABLE_IT(hadc, ADC_IT_AWD);               /* 인터럽트 폭주 방지: main 이 복귀를 확인한 뒤 재활성화 (CR1.AWDIE = 0) */
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

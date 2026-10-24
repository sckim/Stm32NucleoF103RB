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
 * 01_ADC_Injected_HAL_c  —  [계층: HAL]  ADC 주입(Injected) 채널: 연속 변환 중에도 우선 끼어드는 변환
 *
 * 정규(regular) 채널과 주입(injected) 채널의 차이 (RM0008 11장)
 *   정규 채널  : 최대 16개 시퀀스. 결과가 하나의 DR(16비트)에 쌓여 DMA 나 빠른 읽기가 필요하다.
 *   주입 채널  : 최대 4개. 결과가 채널별 전용 JDR1~JDR4 에 저장되어 덮어써지지 않고, 정규 변환을 "중단시키고" 먼저 실행된다.
 *                -> 느린 정규 스캔(예: 센서 여러 개)이 도는 중에도 급한 측정(예: 과전류 감시, 모터 전류 샘플링)을 제때 할 수 있다.
 *
 * 이 예제
 *   정규 채널 : ADC1 CH1 (PA1) 를 연속 변환(DMA 로 최신값을 변수에 저장). PA1 에 가변저항 등을 연결.
 *   주입 채널 : ADC1 CH16 (내부 온도 센서)를 500ms 마다 소프트웨어 트리거로 1회 변환, 변환 완료 인터럽트(JEOC)에서 결과 읽기.
 *   -> 정규 변환이 계속 돌고 있어도 온도 측정이 정확히 500ms 마다 실행되는 것을 확인.
 *   온도 계산 (RM0008 11.10, 데이터시트): T[°C] = (V25 - Vsense)/Avg_Slope + 25,  V25 ≈ 1.43V, Avg_Slope ≈ 4.3mV/°C (개체 편차 큼, 절대값은 ±수 °C)
 *
 * 하드웨어 대응
 *   ADC1->JSQR : 주입 시퀀스(JL 길이, JSQx 채널),  CR2.JEXTSEL=111 + JEXTTRIG : 소프트웨어 트리거 (JSWSTART 로 시작)
 *   ADC1->JOFR1 : 주입 채널 오프셋(결과에서 자동 차감),  ADC1->JDR1 : 결과,  SR.JEOC / CR1.JEOCIE : 주입 변환 완료 플래그/인터럽트
 *   CR2.TSVREFE : 온도센서/Vrefint 활성화 (HAL 이 채널 16 설정 시 켬),  샘플 시간 239.5 사이클(온도센서 최소 17.1us 요구)
 *
 * ISR vs 콜백
 *   ADC1_2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_ADC_IRQHandler()
 *     -> HAL_ADCEx_InjectedConvCpltCallback() (이 파일) : 주입 변환 1회 완료
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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint16_t reg_val;             /* 정규 채널(PA1) 최신값 (DMA 가 계속 덮어씀) */
static volatile uint32_t temp_raw;            /* 주입 채널(온도센서) 결과 */
static volatile uint32_t inj_count;
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
  HAL_ADCEx_Calibration_Start(&hadc1);

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&reg_val, 1) != HAL_OK) Error_Handler();   /* 정규 연속 변환 시작 */

  char msg[96];
  uint32_t t_inj = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GetTick() - t_inj >= 500)
    {
        t_inj += 500;
        if (HAL_ADCEx_InjectedStart_IT(&hadc1) != HAL_OK) Error_Handler();   /* JSWSTART: 정규 변환을 끊고 주입 변환 실행 */
    }

    static uint32_t last_count;
    if (inj_count != last_count)
    {
        last_count = inj_count;
        /* mV = raw x 3300 / 4095 (VDDA=3.3V 가정),  T = (1430 - mV) x 10 / 43 + 25 */
        int32_t mv = (int32_t)(temp_raw * 3300UL / 4095UL);
        int32_t t10 = (1430 - mv) * 100 / 43 + 250;            /* 0.1 °C 단위 */
        int n = snprintf(msg, sizeof msg, "regular(PA1)=%4u  injected(temp raw)=%4lu (%ld mV)  temp=%ld.%ld C  #%lu\r\n",
                         reg_val, (unsigned long)temp_raw, (long)mv, (long)(t10 / 10), (long)(t10 < 0 ? -t10 % 10 : t10 % 10),
                         (unsigned long)inj_count);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
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
  ADC_InjectionConfTypeDef sConfigInjected = {0};

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

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_TEMPSENSOR;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
  sConfigInjected.InjectedNbrOfConversion = 1;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  sConfigInjected.ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.InjectedOffset = 0;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* [설정 요약 -- 레지스터 관점]
   *  ADC 클럭    : PCLK2(64MHz)/6 = 10.67MHz  (SystemClock_Config 의 RCC_ADCPCLK2_DIV6, RCC->CFGR.ADCPRE)
   *  정규 채널   : CH1(PA1), 연속 변환 (CR2.CONT=1), 소프트웨어 시작, 샘플 239.5 사이클 (SMPR2)
   *  주입 채널   : CH16(내부 온도센서) -> ADC1->JSQR (JL=0: 1개), CR2.JEXTSEL=111 + JEXTTRIG: 소프트웨어 트리거 (JSWSTART)
   *                CR1.JAUTO=0 (정규 뒤에 자동 실행 안 함), ADC1->JOFR1=0 (오프셋), 결과는 ADC1->JDR1
   *                CR2.TSVREFE: HAL 이 채널 16 설정 시 켠다. 온도센서 샘플 시간은 최소 17.1us -> 239.5 사이클(약 22us)
   *  DMA        : DMA1_Channel1 원형, 16비트 -> reg_val (MX_DMA_Init / HAL_ADC_MspInit).
   *               DMA 인터럽트(NVIC)는 사용하지 않는다 (최신값만 덮어쓰면 되므로). */
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
/* HAL 콜백 (ISR 문맥): 주입 변환 완료 (SR.JEOC)                                  */
/* ------------------------------------------------------------------------- */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    temp_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);   /* ADC1->JDR1 (오프셋 차감 후) */
    inj_count++;
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

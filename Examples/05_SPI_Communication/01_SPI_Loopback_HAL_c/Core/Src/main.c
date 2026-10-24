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
 * 01_SPI_Loopback_HAL_c  —  [계층: HAL]  SPI1 마스터 루프백 + 클럭 분주별 전송 속도 측정
 *
 * 배선: PA7 (MOSI, 'D11') ---- 점퍼선 ---- PA6 (MISO, 'D12')     (슬레이브 장치 없이 마스터만으로 시험)
 *       SCK = PA5 ('D13') 은 온보드 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다(정상).
 *
 * 동작
 *   - 256바이트 패턴을 SPI 로 송신하면서 동시에 수신(전이중)하고, 송신 == 수신인지 비교해 PASS/FAIL 출력.
 *   - 분주비 /256, /64, /16, /4 로 바꿔 가며 전송 시간을 DWT 사이클로 재고 실제 비트율을 출력.
 *   - PA7-PA6 점퍼를 빼면 수신이 전부 0x00/0xFF 가 되어 FAIL 이 나온다 (음성 대조).
 *
 * SPI 설정 (RM0008 25장)
 *   SPI1 클럭 = APB2 = 64MHz.  SPI1->CR1.BR[2:0] : /2 ~ /256.   /64 -> 1MHz, /4 -> 16MHz
 *   CR1.MSTR=1(마스터), CR1.CPOL=0 / CPHA=0 (모드 0: 유휴 Low, 첫 에지에서 샘플), 8비트, MSB 먼저
 *   CR1.SSM=1, SSI=1 (NSS 를 소프트웨어로 관리)
 *   전이중 1바이트: DR 에 쓰면 송신 시프트 시작 -> 같은 클럭에 수신 -> SR.RXNE 후 DR 읽기
 *
 * 인터럽트/DMA 를 사용하지 않는다 (HAL_SPI_TransmitReceive 폴링). DMA 버전은 02_SPI_DMA_Fullduplex 참고.
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
#define LEN 256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t tx[LEN], rx[LEN];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void SPI1_Reinit(uint32_t prescaler);

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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;         /* DWT 사이클 카운터 켜기 */
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  for (int i = 0; i < LEN; i++) tx[i] = (uint8_t)(i * 7 + 3);

  static const struct { uint32_t reg; uint32_t div; } cfg[] = {
      { SPI_BAUDRATEPRESCALER_256, 256 }, { SPI_BAUDRATEPRESCALER_64, 64 },
      { SPI_BAUDRATEPRESCALER_16, 16 },   { SPI_BAUDRATEPRESCALER_4, 4 },
  };

  char msg[112];
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    for (unsigned k = 0; k < sizeof cfg / sizeof cfg[0]; k++)
    {
        SPI1_Reinit(cfg[k].reg);        /* 분주비 변경 후 SPI1 재초기화 */
        memset(rx, 0, sizeof rx);

        uint32_t t0 = DWT->CYCCNT;
        HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, LEN, 100);
        uint32_t cyc = DWT->CYCCNT - t0;

        int ok = (st == HAL_OK) && (memcmp(tx, rx, LEN) == 0);
        uint32_t bits = LEN * 8U;
        uint32_t kbps = (uint32_t)(((uint64_t)bits * 64000ULL) / cyc);   /* bit / (cyc/64MHz) 를 kbit/s 로 */
        int n = snprintf(msg, sizeof msg, "/%-3lu SCK=%5lu kHz(설정)  측정=%5lu kbit/s (오버헤드 포함)  %s\r\n",
                         (unsigned long)cfg[k].div, (unsigned long)(64000UL / cfg[k].div),
                         (unsigned long)kbps, ok ? "PASS" : "FAIL");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
    }
    HAL_UART_Transmit(&huart2, (uint8_t *)"----\r\n", 6, 100);
    HAL_Delay(2000);
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* SPI1 분주비 변경: HAL_SPI_DeInit(MspDeInit: SPI1EN/핀 해제) -> BR 변경 -> HAL_SPI_Init(MspInit 재실행)
 * SPI1->CR1.BR[2:0] 은 SPE=0 상태에서만 바꿀 수 있으므로 HAL 이 SPE 를 끄고 다시 쓴다.
 * MX_SPI1_Init() 은 CubeMX 가 생성하는 고정값 초기화라, 런타임 변경용 함수를 USER CODE 에 따로 둔다. */
static void SPI1_Reinit(uint32_t prescaler)
{
  if (hspi1.State != HAL_SPI_STATE_RESET) HAL_SPI_DeInit(&hspi1);
  hspi1.Init.BaudRatePrescaler = prescaler;               /* CR1.BR[2:0] */
  if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
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

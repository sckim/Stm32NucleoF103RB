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
 * 04_SPI_Slave_Loopback_HAL_c  —  [계층: HAL]  SPI 슬레이브 모드: 보드 한 장 안에서 SPI1(마스터) <-> SPI2(슬레이브) 시험
 *
 * 배선 (점퍼선 4개)
 *   PA5 (SPI1_SCK)  ---- PB13 (SPI2_SCK)
 *   PA7 (SPI1_MOSI) ---- PB15 (SPI2_MOSI)
 *   PB14 (SPI2_MISO) ---- PA6 (SPI1_MISO)
 *   PA4 (마스터가 GPIO 로 제어하는 CS) ---- PB12 (SPI2_NSS, 슬레이브 하드웨어 NSS 입력)
 *   (PA5 는 온보드 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다)
 *
 * SPI 마스터/슬레이브의 차이
 *   마스터가 SCK 를 만들고 NSS(CS)로 슬레이브를 선택한다. 슬레이브는 SCK 를 받아 데이터를 시프트하며, 마스터가 클럭을 주는 순간
 *   슬레이브의 "송신 데이터가 이미 준비되어 있어야" 한다 (전이중이라 수신과 송신이 같은 클럭에 일어남).
 *   -> 슬레이브는 응답을 미리 장전(arm)해 두고, 한 프레임이 끝나면 다음 프레임 응답을 다시 장전한다.
 *   그래서 슬레이브의 응답은 "직전 프레임에 대한 결과"가 되어 한 프레임 늦게 도착한다 (SPI 센서/메모리가 흔히 쓰는 파이프라인 방식).
 *
 * 동작 프로토콜 (8바이트 프레임)
 *   마스터가 [n, n+1, ..., n+7] 을 보내면, 슬레이브는 그것을 받으면서 동시에 "직전 프레임의 각 바이트를 비트 반전한 값"을 돌려준다.
 *   마스터는 받은 값이 (직전에 보낸 값 ^ 0xFF) 인지 검사해 PASS/FAIL 을 UART(115200)로 출력한다.
 *
 * 하드웨어 대응
 *   SPI2->CR1: MSTR=0(슬레이브), SSM=0(하드웨어 NSS), CPOL=0/CPHA=0(마스터와 동일해야 함).  SPI2 는 APB1(32MHz)에 연결.
 *   마스터 SPI1: 64MHz/64 = 1MHz.   슬레이브 SCK 최대는 f_PCLK/... 이내이면 되므로 1MHz 는 충분.
 *
 * ISR vs 콜백
 *   SPI2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_SPI_IRQHandler()
 *     -> HAL_SPI_TxRxCpltCallback() (이 파일) : 슬레이브가 한 프레임을 다 주고받았을 때 (마스터 쪽은 폴링이라 콜백 없음)
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
#define FRAME 8

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* hspi1 = 마스터, hspi2 = 슬레이브 */
static uint8_t s_tx[FRAME];   /* 슬레이브가 다음 프레임에 내보낼 데이터 */
static uint8_t s_rx[FRAME];   /* 슬레이브가 받은 데이터 */
static volatile uint32_t slave_frames;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
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
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  memset(s_tx, 0x00, sizeof s_tx);
  if (HAL_SPI_TransmitReceive_IT(&hspi2, s_tx, s_rx, FRAME) != HAL_OK) Error_Handler();   /* 슬레이브: 첫 프레임 장전 */

  char msg[128];
  uint8_t sent[FRAME] = {0}, prev_sent[FRAME] = {0}, got[FRAME];
  uint32_t pass = 0, fail = 0;
  uint32_t n = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    memcpy(prev_sent, sent, sizeof sent);
    for (int i = 0; i < FRAME; i++) sent[i] = (uint8_t)(n * 8 + i);

    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);                 /* CS Low: 슬레이브 선택 */
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, sent, got, FRAME, 100);
    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);                   /* CS High: 프레임 종료 */

    HAL_Delay(20);                                                        /* 슬레이브가 다음 프레임을 장전할 시간 */

    int ok = 1;
    if (n > 0)                                                            /* 첫 프레임의 응답은 초기값이라 검사 제외 */
        for (int i = 0; i < FRAME; i++) if (got[i] != (uint8_t)(prev_sent[i] ^ 0xFF)) ok = 0;
    if (st != HAL_OK) ok = 0;
    if (n > 0) { if (ok) pass++; else fail++; }

    int len = snprintf(msg, sizeof msg, "#%lu  sent %02X..%02X  got %02X %02X %02X %02X ...  slave frames=%lu  %s (pass=%lu fail=%lu)\r\n",
                       (unsigned long)n, sent[0], sent[FRAME - 1], got[0], got[1], got[2], got[3],
                       (unsigned long)slave_frames, n == 0 ? "-" : (ok ? "PASS" : "FAIL"),
                       (unsigned long)pass, (unsigned long)fail);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)len, 100);
    HAL_Delay(480);
    n++;
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
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* SPI1 = 마스터. SPI1->CR1: MSTR=1, SSM=1/SSI=1 (NSS 소프트웨어: CS 는 SPI1_CS(PA4) GPIO 로 직접 제어),
   * BR[2:0]=101 (PCLK2 64MHz / 64 = 1MHz), CPOL=0/CPHA=0 (모드 0), MSB first */
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */


  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */


  /* USER CODE END SPI2_Init 1 */
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */
  /* SPI2 = 슬레이브. SPI2->CR1: MSTR=0, SSM=0 (하드웨어 NSS: PB12 가 Low 일 때만 동작),
   * CPOL=0/CPHA=0 (마스터와 동일한 모드 0). BaudRatePrescaler 는 슬레이브에서 무시된다 (SCK 는 마스터가 만든다).
   * SPI2 는 APB1(32MHz) 에 연결. 수신/송신 인터럽트는 HAL_SPI_MspInit 에서 NVIC(SPI2_IRQn) 에 등록. */
  /* USER CODE END SPI2_Init 2 */

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
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 슬레이브가 한 프레임 송수신 완료 -> 다음 응답을 만들어 다시 장전       */
/* ------------------------------------------------------------------------- */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) return;
    for (int i = 0; i < FRAME; i++) s_tx[i] = (uint8_t)(s_rx[i] ^ 0xFF);      /* 다음 프레임에서 돌려줄 응답 = 이번에 받은 값의 반전 */
    slave_frames++;
    HAL_SPI_TransmitReceive_IT(hspi, s_tx, s_rx, FRAME);                      /* 다음 프레임 장전 */
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)                          /* 오버런/모드 오류 시 슬레이브 재장전 */
{
    if (hspi->Instance == SPI2) HAL_SPI_TransmitReceive_IT(hspi, s_tx, s_rx, FRAME);
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

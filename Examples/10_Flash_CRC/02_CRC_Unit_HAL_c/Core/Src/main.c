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
 * 02_CRC_Unit_HAL_c  —  [계층: HAL]  하드웨어 CRC 계산 유닛 (소프트웨어 구현과 비교)
 *
 * STM32F1 CRC 유닛의 사양 (RM0008 4장 CRC calculation unit) — 고정이며 바꿀 수 없다
 *   다항식 0x04C11DB7 (CRC-32/Ethernet 과 동일), 초기값 0xFFFFFFFF, 입력/출력 비트 반전 없음, 최종 XOR 없음
 *   => 표준명 "CRC-32/MPEG-2".  입력은 32비트 워드 단위로 CRC->DR 에 쓰고, 결과는 CRC->DR 에서 읽는다 (1워드 4 AHB 사이클).
 *   * zlib/PNG/ZIP 이 쓰는 표준 CRC-32 는 입력·출력 비트 반전과 최종 XOR 이 있어 값이 다르다.
 *     (이후 STM32 시리즈의 CRC 는 다항식/반전 설정이 가능하지만 F1 은 불가능)
 *
 * 동작
 *   1) 256워드(1KB) 데이터의 CRC 를 HW(HAL_CRC_Calculate)와 SW(비트 단위 루프)로 계산해 결과 일치를 확인 (PASS/FAIL)
 *   2) 두 방식의 소요 시간을 DWT 사이클로 측정해 비교 (HW 가 수십 배 빠름)
 *   3) 데이터 1비트를 바꾸면 CRC 가 완전히 달라지는 것을 확인 (오류 검출 능력)
 *   활용: 펌웨어 무결성 검사(부트로더가 Flash 전체 CRC 확인), 통신 프레임 검사, 플래시 저장 데이터 유효성 표시.
 *
 * 하드웨어 대응
 *   CRC->CR.RESET(bit0)=1 : DR 을 초기값 0xFFFFFFFF 로,  CRC->DR : 쓰면 누적 계산, 읽으면 현재 CRC,  CRC->IDR : 범용 1바이트
 *   RCC->AHBENR.CRCEN(bit6) : 클럭 허용
 *
 * 인터럽트를 사용하지 않는다.
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
#define N_WORDS 256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint32_t data[N_WORDS];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* CRC-32/MPEG-2 의 소프트웨어 구현: 워드를 XOR 한 뒤 MSB 부터 32번 시프트/다항식 XOR */
static uint32_t crc32_mpeg2_sw(const uint32_t *w, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < n; i++)
    {
        crc ^= w[i];
        for (int b = 0; b < 32; b++)
            crc = (crc & 0x80000000UL) ? (crc << 1) ^ 0x04C11DB7UL : (crc << 1);
    }
    return crc;
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
  MX_CRC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  for (uint32_t i = 0; i < N_WORDS; i++) data[i] = 0x9E3779B9UL * (i + 1);   /* 임의 패턴 */

  uint32_t t0 = DWT->CYCCNT;
  uint32_t hw = HAL_CRC_Calculate(&hcrc, data, N_WORDS);      /* CR.RESET 후 DR 에 256워드 기록, DR 읽기 */
  uint32_t hw_cyc = DWT->CYCCNT - t0;

  t0 = DWT->CYCCNT;
  uint32_t sw = crc32_mpeg2_sw(data, N_WORDS);
  uint32_t sw_cyc = DWT->CYCCNT - t0;

  int n = snprintf(msg, sizeof msg, "\r\nHW CRC = 0x%08lX (%lu cycles)\r\nSW CRC = 0x%08lX (%lu cycles)  -> %s, HW is %lux faster\r\n",
                   (unsigned long)hw, (unsigned long)hw_cyc, (unsigned long)sw, (unsigned long)sw_cyc,
                   hw == sw ? "MATCH" : "MISMATCH", (unsigned long)(hw_cyc ? sw_cyc / hw_cyc : 0));
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);

  /* 오류 검출: 1비트만 뒤집어도 CRC 가 완전히 달라진다 */
  data[100] ^= 0x00000001UL;
  uint32_t hw2 = HAL_CRC_Calculate(&hcrc, data, N_WORDS);
  n = snprintf(msg, sizeof msg, "1-bit flip -> CRC = 0x%08lX (%s)\r\n", (unsigned long)hw2,
               hw2 != hw ? "detected" : "NOT detected");
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */
  /* CRC->CR.RESET(bit0) 는 HAL_CRC_Calculate() 가 호출할 때마다 DR 을 0xFFFFFFFF 로 초기화한다 */

  /* USER CODE END CRC_Init 2 */

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

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
 * 01_Flash_Write_c  —  [계층: HAL]  내부 Flash 에 데이터 저장 (EEPROM 에뮬레이션 맛보기)
 *
 * 동작: "부팅 횟수" 를 전원이 꺼져도 유지되도록 내부 Flash 마지막 페이지에 기록한다.
 *   - Flash 는 "지우기(1로 채움) -> 필요한 비트만 0 으로 쓰기" 만 가능하고, 지우기는 페이지(1KB) 단위이다.
 *   - 그래서 값이 바뀔 때마다 지우지 않고, 페이지 안의 "다음 빈 칸(0xFFFFFFFF)" 에 새 값을 덧붙여 쓴다.
 *     (256칸 = 256번 부팅까지 한 번도 지우지 않음 -> Flash 수명(약 10,000회 쓰기/지우기) 절약: 웨어 레벨링)
 *     페이지가 다 차면 그때 지우고 처음부터 다시 시작한다. 현재 값 = 마지막으로 쓰인 칸.
 *   - B1(PC13) 을 누른 채 리셋하면 페이지를 지워 카운터를 초기화한다.
 *
 * 메모리 맵 (STM32F103RB, 128KB Flash)
 *   0x08000000 ~ 0x0801FFFF, 페이지 크기 1KB, 마지막 페이지(127) = 0x0801FC00 ~ 0x0801FFFF  (프로그램은 이보다 앞에 위치)
 *   -> 링커 스크립트의 Flash 크기를 줄여 이 페이지를 데이터 전용으로 예약하는 것이 정석이다 (이 예제는 프로그램이 작아 생략).
 *
 * 하드웨어 대응 (RM0008 3장, PM0075)
 *   FLASH->KEYR 에 KEY1/KEY2 를 써서 잠금 해제 (HAL_FLASH_Unlock),  FLASH->CR.LOCK 으로 다시 잠금
 *   지우기: CR.PER=1, AR=페이지 주소, CR.STRT=1, SR.BSY 대기
 *   쓰기 : CR.PG=1 후 반워드(16비트) 단위로 쓰기 (HAL 의 WORD 프로그램은 반워드 2번)
 *
 * 주의
 *   - 지우기/쓰기 중에는 Flash 접근이 막혀 코드 실행이 멈춘다(지우기 약 20~40ms). 인터럽트 응답이 그만큼 지연된다.
 *   - 쓰기 도중 전원이 끊기면 값이 깨질 수 있다. 실제 제품은 CRC/유효 표시, 2벌 저장 등으로 보호한다.
 *   - ST-Link 로 새 프로그램을 올릴 때 이 페이지가 덮어써지면 카운터가 초기화된다.
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
#define DATA_PAGE_ADDR  0x0801FC00UL        /* 데이터 전용 페이지 = 마지막 페이지(127) */
#define PAGE_SIZE_BYTES 1024UL
#define SLOTS           (PAGE_SIZE_BYTES / 4UL)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t *slot(uint32_t i) { return (uint32_t *)(DATA_PAGE_ADDR + i * 4UL); }

static int flash_erase_data_page(void)
{
    FLASH_EraseInitTypeDef e = {0};
    uint32_t page_error = 0;
    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.PageAddress = DATA_PAGE_ADDR;
    e.NbPages = 1;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&e, &page_error);
    HAL_FLASH_Lock();
    return st == HAL_OK;
}

static int flash_write_word(uint32_t addr, uint32_t value)
{
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, value);
    HAL_FLASH_Lock();
    return st == HAL_OK && *(volatile uint32_t *)addr == value;         /* 읽어서 검증 */
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char msg[96];
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
  if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)          /* B1 누른 채 부팅: 카운터 초기화 */
  {
      int ok = flash_erase_data_page();
      int n = snprintf(msg, sizeof msg, "\r\nB1 held: data page erased (%s)\r\n", ok ? "ok" : "FAILED");
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
  }

  /* 마지막으로 쓰인 칸 찾기: 앞에서부터 0xFFFFFFFF 가 아닌 마지막 칸 */
  int32_t last = -1;
  for (uint32_t i = 0; i < SLOTS; i++)
  {
      if (*slot(i) != 0xFFFFFFFFUL) last = (int32_t)i; else break;
  }

  uint32_t count = (last >= 0) ? *slot((uint32_t)last) + 1U : 1U;
  int32_t next = last + 1;
  int erased = 0;
  if (next >= (int32_t)SLOTS)                    /* 페이지가 가득 참 -> 지우고 처음 칸부터 */
  {
      if (!flash_erase_data_page()) Error_Handler();
      next = 0;
      erased = 1;
  }
  int ok = flash_write_word((uint32_t)slot((uint32_t)next), count);

  int n = snprintf(msg, sizeof msg, "\r\nboot count = %lu  (stored in slot %ld/%lu at 0x%08lX)%s  write:%s\r\n",
                   (unsigned long)count, (long)next, (unsigned long)SLOTS,
                   (unsigned long)slot((uint32_t)next), erased ? " [page wrapped]" : "", ok ? "ok" : "FAILED");
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);   /* GPIOA->ODR bit5 반전 */
    HAL_Delay(500);
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

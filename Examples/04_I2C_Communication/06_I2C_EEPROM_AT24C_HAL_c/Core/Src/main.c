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
 * 06_I2C_EEPROM_AT24C_HAL_c  —  [계층: HAL]  I2C EEPROM(AT24C32/64 등) 읽기/쓰기: 16비트 주소, 페이지 경계, ACK 폴링
 *
 * 배선: AT24C32/AT24C64 모듈(또는 ZS-042 RTC 모듈의 EEPROM): VCC=3.3V, GND, SCL=PB6, SDA=PB7
 *   7비트 주소 = 0x50 | (A2 A1 A0).  모듈 기본은 A2..A0 = 000 -> 0x50.  (ZS-042 는 EEPROM 이 0x57 인 경우가 많다 -> EE_ADDR 수정)
 *
 * AT24Cxx 의 규칙
 *   1) 메모리 주소가 **16비트**(AT24C32/64: 4KB/8KB)라서 HAL 에서 `I2C_MEMADD_SIZE_16BIT` 로 주소 2바이트를 보낸다. (AT24C02/04/08/16 은 8비트)
 *   2) **페이지 쓰기**: 한 번에 쓸 수 있는 최대 크기가 "페이지"(AT24C32/64 는 32바이트). 페이지 경계를 넘으면 주소가
 *      같은 페이지의 처음으로 되감겨(wrap-around) 앞쪽 데이터를 덮어쓴다 -> 페이지 단위로 쪼개서 써야 한다.
 *   3) **쓰기 사이클 시간**: 쓰기 명령 뒤 EEPROM 이 내부에서 셀을 프로그래밍하는 동안(최대 5ms) 주소에 NACK 한다.
 *      고정 delay 대신 **ACK 폴링**(HAL_I2C_IsDeviceReady 반복)으로 끝나는 즉시 다음 동작을 하면 빠르고 안전하다.
 *   4) 읽기는 페이지 제한 없이 연속으로 가능(주소 자동 증가).  수명: 셀당 약 100만 회 쓰기 -> 자주 바뀌는 값은 같은 주소에 반복 쓰지 말 것.
 *
 * 동작: 페이지 경계를 걸치는 40바이트(0x011C부터, 0x0120 이 페이지 경계)를 쓰고 다시 읽어 비교 -> PASS/FAIL 을 UART(115200)로 출력.
 *       쓰기 시간(ACK 폴링 횟수 포함)도 함께 표시. 영역 0x0100~0x01FF 만 사용한다(다른 데이터는 건드리지 않음).
 *
 * 인터럽트를 사용하지 않는다 (블로킹 API).
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
#define EE_ADDR   (0x50 << 1)          /* HAL 은 7비트 주소를 1비트 왼쪽으로 민 값을 요구 */
#define EE_PAGE   32U                  /* AT24C32/64 페이지 크기 (AT24C256 = 64, AT24C02 = 8 ...) */
#define TEST_ADDR 0x011CU
#define TEST_LEN  40U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 쓰기 완료를 ACK 폴링으로 기다린다. 반환: 폴링 횟수 (실패 시 0) */
static uint32_t ee_wait_ready(void)
{
    uint32_t t0 = HAL_GetTick(), polls = 0;
    while ((HAL_GetTick() - t0) < 20)
    {
        polls++;
        if (HAL_I2C_IsDeviceReady(&hi2c1, EE_ADDR, 1, 5) == HAL_OK) return polls;   /* ACK 오면 내부 쓰기 종료 */
    }
    return 0;
}

/* 페이지 경계를 넘지 않게 나눠 쓰기 */
static int ee_write(uint16_t addr, const uint8_t *data, uint16_t len, uint32_t *polls_total)
{
    while (len)
    {
        uint16_t room = (uint16_t)(EE_PAGE - (addr % EE_PAGE));         /* 이 페이지에 남은 바이트 */
        uint16_t n = len < room ? len : room;
        if (HAL_I2C_Mem_Write(&hi2c1, EE_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)data, n, 100) != HAL_OK) return 0;
        uint32_t p = ee_wait_ready();
        if (!p) return 0;
        *polls_total += p;
        addr = (uint16_t)(addr + n); data += n; len = (uint16_t)(len - n);
    }
    return 1;
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
    uint32_t round = 0;       /* 테스트 라운드 카운터 (원본: for 루프 변수) */
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
    if (HAL_I2C_IsDeviceReady(&hi2c1, EE_ADDR, 3, 20) != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\nEEPROM not found (check address/wiring)\r\n", 42, 100);
        Error_Handler();
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        uint8_t wr[TEST_LEN], rd[TEST_LEN];
        for (uint32_t i = 0; i < TEST_LEN; i++) wr[i] = (uint8_t)(round * 3 + i);      /* 라운드마다 다른 패턴 */
        memset(rd, 0, sizeof rd);

        uint32_t polls = 0;
        uint32_t t0 = HAL_GetTick();
        int okw = ee_write(TEST_ADDR, wr, TEST_LEN, &polls);
        uint32_t tw = HAL_GetTick() - t0;

        int okr = (HAL_I2C_Mem_Read(&hi2c1, EE_ADDR, TEST_ADDR, I2C_MEMADD_SIZE_16BIT, rd, TEST_LEN, 100) == HAL_OK);
        int match = okw && okr && (memcmp(wr, rd, TEST_LEN) == 0);

        int n = snprintf(msg, sizeof msg, "#%lu write %u B @0x%04X (page 경계 걸침) %lu ms, ACK polls=%lu | read back: %s\r\n",
                         (unsigned long)round, TEST_LEN, TEST_ADDR, (unsigned long)tw, (unsigned long)polls,
                         match ? "PASS" : "FAIL");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(2000);
        round++;
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */
  /* HAL_I2C_Init -> (MspInit) 후 I2C1->CR2.FREQ = PCLK1(MHz), I2C1->CCR / TRISE = 100kHz 타이밍,
   * CR1.PE = 1. NoStretch/GeneralCall/DualAddress = OAR2.ENDUAL, CR1.NOSTRETCH/ENGC 비트 */
  /* USER CODE END I2C1_Init 2 */

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

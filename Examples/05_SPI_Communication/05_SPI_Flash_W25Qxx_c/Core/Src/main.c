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
 * 05_SPI_Flash_W25Qxx_c  —  [계층: HAL]  SPI NOR Flash(W25Q16/32/64/128 등) 제어: ID 읽기, 소거, 페이지 프로그램, 읽기
 *
 * 배선 (W25Qxx 모듈, 3.3V): VCC=3.3V, GND, CLK=PA5, DI(MOSI)=PA7, DO(MISO)=PA6, CS=PA4.  /WP, /HOLD 는 3.3V 로 묶는다.
 *   PA5 는 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다.
 *
 * NOR Flash 의 규칙 (내부 Flash 와 같다, 01_Flash_Write 참고)
 *   - 쓰기 = 비트를 1 -> 0 으로만 바꾼다. 다시 1 로 되돌리려면 **소거**(섹터 4KB 단위, 값이 0xFF 가 됨)가 필요하다.
 *   - **페이지 프로그램**은 한 번에 최대 256바이트, 페이지(256B) 경계를 넘으면 주소가 되감긴다.
 *   - 쓰기/소거는 시간이 걸린다(페이지 프로그램 약 0.7ms, 섹터 소거 약 45ms). 그동안 상태 레지스터의 **BUSY(WIP) 비트**를 폴링한다.
 *   - 모든 쓰기/소거 명령 전에는 **WEL(쓰기 허용)** 래치를 켜는 `0x06 Write Enable` 이 필요하고, 명령이 끝나면 자동으로 꺼진다.
 *
 * 사용하는 명령
 *   0x9F JEDEC ID (제조사, 종류, 용량: W25Q64 = EF 40 17, W25Q128 = EF 40 18),  0x05 Read Status-1 (bit0 = BUSY, bit1 = WEL),
 *   0x06 Write Enable,  0x20 Sector Erase(4KB),  0x02 Page Program,  0x03 Read Data,  0x4B Read Unique ID(64비트)
 *
 * 동작: JEDEC ID/용량/고유 ID 출력 -> 테스트 섹터(주소 0x001000, 4KB)를 소거 -> 소거 확인(전부 0xFF) -> 256바이트 패턴 프로그램
 *       -> 읽어서 비교. *** 주소 0x001000~0x001FFF 4KB 의 내용이 지워진다(다른 영역은 건드리지 않음). 중요한 데이터가 있으면 TEST_ADDR 변경. ***
 *
 * 인터럽트를 사용하지 않는다 (블로킹 SPI).
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
#define TEST_ADDR 0x001000UL           /* 4KB 섹터 경계 */

#define CS_LOW()   HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

/* ---------------- 저수준: 명령 = CS Low -> 바이트들 -> CS High ---------------- */
static uint8_t read_status(void)
{
    uint8_t cmd = 0x05, sr = 0;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    HAL_SPI_Receive(&hspi1, &sr, 1, 10);
    CS_HIGH();
    return sr;
}

static int wait_ready(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (read_status() & 0x01)                                   /* BUSY */
        if (HAL_GetTick() - t0 > timeout_ms) return 0;
    return 1;
}

static void write_enable(void)
{
    uint8_t cmd = 0x06;
    CS_LOW(); HAL_SPI_Transmit(&hspi1, &cmd, 1, 10); CS_HIGH();
}

static void read_jedec(uint8_t id[3])
{
    uint8_t cmd = 0x9F;
    CS_LOW(); HAL_SPI_Transmit(&hspi1, &cmd, 1, 10); HAL_SPI_Receive(&hspi1, id, 3, 10); CS_HIGH();
}

static void read_unique_id(uint8_t uid[8])
{
    uint8_t cmd[5] = { 0x4B, 0, 0, 0, 0 };                          /* 명령 + 더미 4바이트 */
    CS_LOW(); HAL_SPI_Transmit(&hspi1, cmd, 5, 10); HAL_SPI_Receive(&hspi1, uid, 8, 10); CS_HIGH();
}

static int sector_erase(uint32_t addr)
{
    uint8_t cmd[4] = { 0x20, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    write_enable();
    CS_LOW(); HAL_SPI_Transmit(&hspi1, cmd, 4, 10); CS_HIGH();
    return wait_ready(500);                                        /* 4KB 소거 전형 45ms, 최대 400ms */
}

static int page_program(uint32_t addr, const uint8_t *data, uint16_t len)   /* len <= 256, 페이지 경계 넘지 말 것 */
{
    uint8_t cmd[4] = { 0x02, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    write_enable();
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 10);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, 50);
    CS_HIGH();
    return wait_ready(10);                                         /* 페이지 프로그램 전형 0.7ms, 최대 3ms */
}

static void read_data(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint8_t cmd[4] = { 0x03, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 10);
    HAL_SPI_Receive(&hspi1, buf, len, 100);                        /* 주소가 자동으로 증가하며 연속으로 읽힌다 */
    CS_HIGH();
}
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
  char m[128];
  uint8_t id[3];
  read_jedec(id);
  if (id[0] == 0x00 || id[0] == 0xFF)
  {
      print("\r\nno SPI flash detected (JEDEC ID = 00/FF): check wiring/CS\r\n");
      Error_Handler();
  }
  uint32_t cap_kb = (id[2] >= 0x11 && id[2] < 0x20) ? (1UL << (id[2] - 10)) : 0;   /* 용량 코드 n -> 2^n 바이트 */
  uint8_t uid[8];
  read_unique_id(uid);
  snprintf(m, sizeof m, "\r\nJEDEC ID = %02X %02X %02X  (mfr 0x%02X%s, capacity %lu KB)\r\n", id[0], id[1], id[2], id[0],
           id[0] == 0xEF ? " = Winbond" : "", (unsigned long)cap_kb);
  print(m);
  snprintf(m, sizeof m, "unique ID = %02X%02X%02X%02X%02X%02X%02X%02X\r\n", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
  print(m);

  /* 1) 섹터 소거 */
  uint32_t t0 = HAL_GetTick();
  int ok = sector_erase(TEST_ADDR);
  uint32_t t_erase = HAL_GetTick() - t0;

  /* 2) 소거 확인: 4KB 가 전부 0xFF */
  uint8_t buf[256];
  int erased = ok;
  for (uint32_t off = 0; off < 4096 && erased; off += 256)
  {
      read_data(TEST_ADDR + off, buf, 256);
      for (int i = 0; i < 256; i++) if (buf[i] != 0xFF) { erased = 0; break; }
  }

  /* 3) 페이지 프로그램 + 4) 읽어서 비교 */
  uint8_t pat[256], back[256];
  for (int i = 0; i < 256; i++) pat[i] = (uint8_t)(i ^ 0xA5);
  t0 = HAL_GetTick();
  int okp = erased && page_program(TEST_ADDR, pat, 256);
  uint32_t t_prog = HAL_GetTick() - t0;
  read_data(TEST_ADDR, back, 256);
  int match = okp && (memcmp(pat, back, 256) == 0);

  snprintf(m, sizeof m, "sector erase %s (%lu ms), erased-check %s, page program %s (%lu ms), verify %s\r\n",
           ok ? "ok" : "TIMEOUT", (unsigned long)t_erase, erased ? "OK" : "FAIL",
           okp ? "ok" : "FAIL", (unsigned long)t_prog, match ? "PASS" : "FAIL");
  print(m);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : FLASH_CS_Pin */
  GPIO_InitStruct.Pin = FLASH_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(FLASH_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
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

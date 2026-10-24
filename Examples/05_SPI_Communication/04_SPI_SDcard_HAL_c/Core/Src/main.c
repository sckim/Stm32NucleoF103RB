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
 * 04_SPI_SDcard_HAL_c  —  [계층: HAL]  SPI 모드로 SD 카드 초기화와 섹터 읽기 (파일시스템 없이 raw 블록 접근)
 *
 * 배선 (마이크로 SD 모듈, SPI 모드, 3.3V 전용 모듈 사용)
 *   VCC=3.3V, GND
 *   SCK = PA5 (SPI1_SCK, LD2 와 공유),  MOSI = PA7,  MISO = PA6,  CS = PA4 (GPIO)
 *   MISO 에 10kΩ 풀업을 두면 카드가 없을 때도 선이 안정적이다.
 *
 * SD 카드 SPI 모드 초기화 순서 (SD 물리 계층 규격)
 *   1) 카드 선택 해제(CS=High) 상태로 SCK 74클럭 이상, 클럭 400kHz 이하 (아래에서는 250kHz 로 시작)
 *   2) CMD0 (GO_IDLE_STATE)  -> R1 = 0x01 (idle) : 이 명령으로 SPI 모드가 선택된다 (CRC 0x95 필수)
 *   3) CMD8 (SEND_IF_COND, 인자 0x1AA) -> 응답 R7 : 전압 범위/체크 패턴 확인. (v2 카드 이상, CRC 0x87)
 *   4) ACMD41 (= CMD55 + CMD41, 인자 HCS=0x40000000) 를 R1 = 0x00 이 될 때까지 반복 : 카드 내부 초기화 완료 대기
 *   5) CMD58 (READ_OCR) : OCR 의 CCS 비트(0x40000000)가 1 이면 SDHC/SDXC (블록 주소 지정), 0 이면 SDSC (바이트 주소 지정)
 *   6) 이후 클럭을 올린다(여기서는 8MHz)
 *   블록 읽기: CMD17 (READ_SINGLE_BLOCK, 인자 = 섹터 번호(SDHC) 또는 바이트 주소(SDSC)) -> R1 -> 데이터 토큰 0xFE -> 512바이트 -> CRC 2바이트
 *   용량:      CMD9 (SEND_CSD) 로 16바이트 CSD 를 읽어 C_SIZE 로 계산
 *
 * 동작: 초기화 -> 카드 종류/용량/OCR 출력 -> 섹터 0 을 읽어 앞부분 16바이트와 MBR 서명(0x55AA)/첫 파티션 정보를 UART(115200)로 출력.
 *       *** 이 예제는 읽기만 한다. 카드 내용을 바꾸지 않는다. ***
 * 이후 단계: FAT 파일시스템(FatFs, 프레임워크 Middlewares/Third_Party/FatFs)은 이 read/write 블록 함수 위에 붙인다.
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
#define CS_LOW()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static int is_sdhc;             /* 1 = 블록(섹터) 주소, 0 = 바이트 주소 */
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
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

/* ---------------- SPI 1바이트 교환 ---------------- */
static uint8_t xfer(uint8_t b)
{
    uint8_t r = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &b, &r, 1, 20);
    return r;
}

static void deselect(void) { CS_HIGH(); xfer(0xFF); }       /* 카드가 MISO 를 놓도록 추가 클럭 8개 */

/* 명령 전송 후 R1 응답 반환 (0xFF = 응답 없음). CS 는 호출자가 관리 */
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t crc = (cmd == 0) ? 0x95 : (cmd == 8) ? 0x87 : 0x01;   /* SPI 모드에서는 CMD0, CMD8 만 CRC 검사 */
    xfer(0xFF);                                             /* 준비 클럭 */
    xfer((uint8_t)(0x40 | cmd));
    xfer((uint8_t)(arg >> 24)); xfer((uint8_t)(arg >> 16)); xfer((uint8_t)(arg >> 8)); xfer((uint8_t)arg);
    xfer(crc);
    uint8_t r = 0xFF;
    for (int i = 0; i < 10 && (r & 0x80); i++) r = xfer(0xFF);   /* R1 은 최상위 비트가 0 인 첫 바이트 */
    return r;
}

/* 초기화. 성공 시 1 */
static int sd_init(void)
{
    SPI1_Reinit(SPI_BAUDRATEPRESCALER_256);             /* 64MHz/256 = 250kHz (초기화는 400kHz 이하) */
    CS_HIGH();
    for (int i = 0; i < 10; i++) xfer(0xFF);                /* 80 클럭 */

    CS_LOW();
    if (sd_cmd(0, 0) != 0x01) { deselect(); print("CMD0 failed (no card / wiring)\r\n"); return 0; }

    uint8_t r7[4] = {0};
    is_sdhc = 0;
    if (sd_cmd(8, 0x1AA) == 0x01)                           /* v2 카드 */
    {
        for (int i = 0; i < 4; i++) r7[i] = xfer(0xFF);
        if (r7[2] != 0x01 || r7[3] != 0xAA) { deselect(); print("CMD8 echo mismatch\r\n"); return 0; }

        uint32_t t0 = HAL_GetTick();
        uint8_t r;
        do { sd_cmd(55, 0); r = sd_cmd(41, 0x40000000); }   /* ACMD41 with HCS */
        while (r != 0x00 && (HAL_GetTick() - t0) < 1000);
        if (r != 0x00) { deselect(); print("ACMD41 timeout\r\n"); return 0; }

        if (sd_cmd(58, 0) == 0x00)                          /* OCR 읽기 */
        {
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
            is_sdhc = (ocr[0] & 0x40) != 0;                 /* CCS 비트 */
            char m[64];
            snprintf(m, sizeof m, "OCR = %02X%02X%02X%02X\r\n", ocr[0], ocr[1], ocr[2], ocr[3]);
            print(m);
        }
    }
    else                                                    /* v1 카드/MMC: 이 예제는 v2 이상만 지원 */
    {
        deselect(); print("v1 card not supported by this example\r\n"); return 0;
    }
    deselect();
    SPI1_Reinit(SPI_BAUDRATEPRESCALER_8);               /* 이후 8MHz */
    return 1;
}

/* 512바이트 블록 하나 읽기. 성공 시 1 */
static int sd_read_block(uint32_t sector, uint8_t *buf)
{
    CS_LOW();
    if (sd_cmd(17, is_sdhc ? sector : sector * 512UL) != 0x00) { deselect(); return 0; }
    uint32_t t0 = HAL_GetTick();
    uint8_t tok;
    do { tok = xfer(0xFF); } while (tok == 0xFF && (HAL_GetTick() - t0) < 200);   /* 데이터 토큰 대기 */
    if (tok != 0xFE) { deselect(); return 0; }
    for (int i = 0; i < 512; i++) buf[i] = xfer(0xFF);
    xfer(0xFF); xfer(0xFF);                                 /* CRC 2바이트 (버림) */
    deselect();
    return 1;
}

/* CSD 로 용량(MB) 계산. 실패 시 0 */
static uint32_t sd_capacity_mb(void)
{
    uint8_t csd[16];
    CS_LOW();
    if (sd_cmd(9, 0) != 0x00) { deselect(); return 0; }
    uint32_t t0 = HAL_GetTick();
    uint8_t tok;
    do { tok = xfer(0xFF); } while (tok == 0xFF && (HAL_GetTick() - t0) < 200);
    if (tok != 0xFE) { deselect(); return 0; }
    for (int i = 0; i < 16; i++) csd[i] = xfer(0xFF);
    xfer(0xFF); xfer(0xFF);
    deselect();

    if ((csd[0] >> 6) == 1)                                 /* CSD v2 (SDHC/SDXC): 용량 = (C_SIZE+1) x 512KB */
    {
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        return (c_size + 1) / 2;                            /* (C_SIZE+1) x 512KB = (C_SIZE+1)/2 MB */
    }
    uint32_t c_size = ((uint32_t)(csd[6] & 3) << 10) | ((uint32_t)csd[7] << 2) | (csd[8] >> 6);   /* CSD v1 */
    uint32_t mult = ((csd[9] & 3) << 1) | (csd[10] >> 7);
    uint32_t blen = csd[5] & 0x0F;
    return (uint32_t)(((uint64_t)(c_size + 1) << (mult + 2 + blen)) >> 20);
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
  print("\r\n[SD card over SPI] initializing...\r\n");
  if (!sd_init()) { print("init failed\r\n"); Error_Handler(); }

  char m[128];
  snprintf(m, sizeof m, "card type: %s,  capacity: %lu MB\r\n", is_sdhc ? "SDHC/SDXC (block addressing)" : "SDSC (byte addressing)",
           (unsigned long)sd_capacity_mb());
  print(m);

  static uint8_t sec[512];
  if (!sd_read_block(0, sec)) { print("read sector 0 failed\r\n"); Error_Handler(); }

  print("sector 0, first 16 bytes:");
  for (int i = 0; i < 16; i++) { snprintf(m, sizeof m, " %02X", sec[i]); print(m); }
  print("\r\n");
  snprintf(m, sizeof m, "signature @510 = %02X %02X  (%s)\r\n", sec[510], sec[511],
           (sec[510] == 0x55 && sec[511] == 0xAA) ? "valid MBR/boot sector" : "no MBR signature");
  print(m);
  if (sec[510] == 0x55 && sec[511] == 0xAA)
  {
      uint32_t lba = sec[454] | (sec[455] << 8) | (sec[456] << 16) | ((uint32_t)sec[457] << 24);
      snprintf(m, sizeof m, "partition 1: type=0x%02X  start LBA=%lu\r\n", sec[450], (unsigned long)lba);
      print(m);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(1000);                          /* (LD2 는 SPI SCK 와 같은 핀이라 점멸 표시를 쓰지 않는다) */
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
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

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

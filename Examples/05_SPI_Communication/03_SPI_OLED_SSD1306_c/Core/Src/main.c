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
 * 03_SPI_OLED_SSD1306_c  —  [계층: HAL]  SPI 로 SSD1306 128x64 OLED 구동 (7핀 SPI 모듈)
 *
 * 04_I2C_Communication/05_I2C_OLED_SSD1306 과 같은 화면(프레임버퍼, 선, 공, 프레임 번호/FPS)을 SPI 로 그린다. 통신 방식 차이가 핵심이다.
 *   I2C : 제어 바이트(0x00/0x40)가 명령/데이터를 구분, 최대 400kHz 라 한 프레임 약 23ms
 *   SPI : 별도 D/C 핀(Data/Command)이 명령/데이터를 구분, 8MHz 이상 가능해 한 프레임 약 1ms -> 훨씬 빠른 애니메이션
 *
 * 배선 (7핀 SPI OLED 모듈: GND VCC SCL SDA RES DC CS)
 *   GND, VCC=3.3V
 *   SCL(SCK)  = PA5 (SPI1_SCK, 'D13' — 온보드 LD2 와 같은 핀이라 LED 가 흐릿하게 켜진다)
 *   SDA(MOSI) = PA7 (SPI1_MOSI, 'D11')
 *   RES       = PB1  (하드웨어 리셋, 시작할 때 Low 펄스)
 *   DC        = PB0  (Low = 명령, High = 표시 데이터)
 *   CS        = PA4  (칩 선택, Low 에서 유효. 소프트웨어 GPIO 로 제어)
 *
 * SPI 설정: 마스터, 모드 0 (CPOL=0, CPHA=0), 8비트, MSB 먼저, 클럭 = 64MHz/8 = 8MHz (SSD1306 최대 약 10MHz)
 *   SPI1->CR1: MSTR=1, BR=010(/8), CPOL=0, CPHA=0, SSM=1, SSI=1
 *
 * 글꼴은 3x5 픽셀로 숫자/일부 문자만 내장했다 (I2C 예제와 동일). 인터럽트를 사용하지 않는다 (블로킹 SPI, DMA 로 바꾸려면
 * 05_SPI_Communication/02_SPI_DMA_Fullduplex 참고).
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
#define OLED_W 128
#define OLED_H 64

#define CS_LOW()   HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET)
#define DC_CMD()   HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET)
#define DC_DATA()  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t fb[OLED_W * OLED_H / 8];
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
/* ---------------- SSD1306 저수준 (SPI) ---------------- */
static void oled_cmds(const uint8_t *c, uint16_t n)
{
    DC_CMD();  CS_LOW();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)c, n, 100);
    CS_HIGH();
}

static void oled_init(void)
{
    HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_RESET);   /* RES: Low 펄스로 리셋 */
    HAL_Delay(10);
    HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };                                                      /* 각 명령의 의미는 I2C 예제 참고 */
    oled_cmds(init, sizeof init);
}

static void oled_flush(void)
{
    static const uint8_t win[] = { 0x21, 0, OLED_W - 1, 0x22, 0, 7 };
    oled_cmds(win, sizeof win);
    DC_DATA();  CS_LOW();
    HAL_SPI_Transmit(&hspi1, fb, sizeof fb, 100);           /* 1024바이트 = 8MHz 에서 약 1ms */
    CS_HIGH();
}

/* ---------------- 그리기 (프레임버퍼) ---------------- */
static void px(int x, int y, int on)
{
    if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) return;
    uint8_t *b = &fb[(y >> 3) * OLED_W + x];
    if (on) *b |= (uint8_t)(1u << (y & 7)); else *b &= (uint8_t)~(1u << (y & 7));
}

static void fill_rect(int x, int y, int w, int h, int on)
{
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, on);
}

static void rect(int x, int y, int w, int h)
{
    for (int i = 0; i < w; i++) { px(x + i, y, 1); px(x + i, y + h - 1, 1); }
    for (int j = 0; j < h; j++) { px(x, y + j, 1); px(x + w - 1, y + j, 1); }
}

static void line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        px(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static const struct { char c; uint8_t row[5]; } font[] = {
    {'0',{7,5,5,5,7}}, {'1',{2,6,2,2,7}}, {'2',{7,1,7,4,7}}, {'3',{7,1,7,1,7}}, {'4',{5,5,7,1,1}},
    {'5',{7,4,7,1,7}}, {'6',{7,4,7,5,7}}, {'7',{7,1,2,2,2}}, {'8',{7,5,7,5,7}}, {'9',{7,5,7,1,7}},
    {':',{0,2,0,2,0}}, {'.',{0,0,0,0,2}}, {'-',{0,0,7,0,0}}, {' ',{0,0,0,0,0}},
    {'F',{7,4,6,4,4}}, {'P',{7,5,7,4,4}}, {'S',{7,4,7,1,7}},
};

static void text(int x, int y, int scale, const char *s)
{
    for (; *s; s++, x += 4 * scale)
    {
        for (unsigned k = 0; k < sizeof font / sizeof font[0]; k++)
        {
            if (font[k].c != *s) continue;
            for (int r = 0; r < 5; r++)
                for (int c = 0; c < 3; c++)
                    if ((font[k].row[r] >> (2 - c)) & 1) fill_rect(x + c * scale, y + r * scale, scale, scale, 1);
            break;
        }
    }
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
  oled_init();

  int bx = 20, by = 20, vx = 2, vy = 1;
  uint32_t frame = 0, fps = 0, frames_this_sec = 0, t_sec = HAL_GetTick();
  char buf[24];
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    memset(fb, 0, sizeof fb);
    rect(0, 0, OLED_W, OLED_H);
    line(0, 0, 127, 63);
    line(0, 63, 127, 0);

    bx += vx; by += vy;
    if (bx < 2 || bx > OLED_W - 6) { vx = -vx; bx += vx; }
    if (by < 2 || by > OLED_H - 6) { vy = -vy; by += vy; }
    fill_rect(bx, by, 4, 4, 1);

    snprintf(buf, sizeof buf, "%lu", (unsigned long)frame);
    text(6, 6, 3, buf);
    snprintf(buf, sizeof buf, "FPS %lu", (unsigned long)fps);
    text(6, 46, 2, buf);

    oled_flush();

    frame++; frames_this_sec++;
    if (HAL_GetTick() - t_sec >= 1000)
    {
        t_sec += 1000;
        fps = frames_this_sec; frames_this_sec = 0;
        char m[40];
        int n = snprintf(m, sizeof m, "FPS = %lu\r\n", (unsigned long)fps);
        HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 100);
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
  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, OLED_DC_Pin|OLED_RES_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : OLED_CS_Pin */
  GPIO_InitStruct.Pin = OLED_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(OLED_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OLED_DC_Pin OLED_RES_Pin */
  GPIO_InitStruct.Pin = OLED_DC_Pin|OLED_RES_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
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

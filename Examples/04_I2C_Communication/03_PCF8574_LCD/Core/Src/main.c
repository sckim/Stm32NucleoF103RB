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
 * 03_PCF8574_LCD  —  [계층: HAL + 사용자 드라이버(i2c_lcd)]  I2C 백팩(PCF8574)을 단 문자 LCD 제어
 *
 * 구성
 *   I2C1 (100kHz, PB8/PB9 로 리맵) -> PCF8574 (I/O 확장기) -> HD44780 문자 LCD (4비트 모드)
 *   i2c_lcd.h : 외부 라이브러리 I2C-LCD-STM32 (PlatformIO lib_deps) — HAL I2C 호출을 감싼 LCD 드라이버
 *
 * 하드웨어 대응 (RM0008)
 *   AFIO_MAPR.I2C1_REMAP = 1 : I2C1 을 PB6/PB7 에서 PB8/PB9 로 이동 (HAL_msp 에서 설정)
 *   I2C1_CR1, CCR, TRISE      : 100kHz 표준 모드 타이밍
 *   HAL_I2C_IsDeviceReady     : 주소 바이트를 보내 ACK 여부를 확인 (I2C 스캐너)
 *   LCD 주소 0x4E = 7비트 0x27 << 1  (HAL 은 8비트 형태 주소를 요구)
 *
 * I2C 는 폴링 방식의 블로킹 전송이다 (I2C 인터럽트/콜백 없음).
 * 활성화된 ISR (stm32f1xx_it.c)
 *   SysTick_Handler      : 1ms 틱 (HAL_IncTick, HAL_Delay/타임아웃용)
 *   EXTI15_10_IRQHandler : B1(PC13) 상승 에지 (CubeMX 기본 설정, EXTI_IMR.MR13).
 *                          HAL_GPIO_EXTI_IRQHandler -> HAL_GPIO_EXTI_Callback 을 부르지만
 *                          이 예제는 콜백을 재정의하지 않으므로 weak 빈 함수가 실행되고 아무 일도 없다.
 * 주의: "STM32 with C Code" 는 17자이므로 16x2 LCD 에서는 마지막 'e'(DDRAM 0x10)가 보이지 않는다.
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "i2c_lcd.h" // 다운로드된 드라이버 헤더 결합
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
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// 1. LCD 핸들 인스턴스 선언 (전역 또는 main 상단)
I2C_LCD_HandleTypeDef lcd1;
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
// 아래 코드가 있어야만 printf했을 때 UART2로 출력된다.
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE {
	if (ch == '\n')
		HAL_UART_Transmit(&huart2, (uint8_t*) "\r", 1, 0xFFFF);
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF);
	return ch;
}

/*
 * HD44780 4비트 초기화 (라이브러리 lcd_init() 대체)
 *
 * PCF8574 출력 비트 배치 (모듈 표준, 라이브러리와 동일)
 *   P0 = RS, P1 = RW, P2 = E, P3 = 백라이트, P4~P7 = D4~D7
 *
 * 라이브러리 lcd_init() 의 문제
 *   lcd_send_cmd(0x30) 은 니블을 두 번(3, 0) 보낸다. 전원 직후 LCD 는 8비트 모드라서
 *   E 펄스 한 번이 명령 하나이므로 "0x3?" 와 "0x0?" 두 명령이 된다.
 *   lcd_send_cmd(0x20) 도 2 와 0 두 니블을 보낸다. 2 에서 4비트 모드로 바뀐 뒤 남은 0 이
 *   다음 명령의 상위 니블이 되어, 이후 모든 명령의 니블 짝이 하나씩 어긋난다.
 *   그 결과 0x28(2줄), 0x0C(표시 ON)가 실행되지 않아 화면이 꺼진 채로 남는다
 *   (Proteus LM016L 에서 화면에 아무것도 나오지 않는 원인).
 *
 * 해결: 데이터시트(HD44780 Figure 24) 순서대로 3, 3, 3, 2 를 "니블 하나씩" 보낸 뒤
 *       4비트 모드에서 라이브러리 lcd_send_cmd() 로 설정 명령을 보낸다.
 */
static void lcd_write_nibble(I2C_LCD_HandleTypeDef *lcd, uint8_t nibble)
{
	uint8_t d = (uint8_t)(nibble << 4);
	uint8_t buf[2] = {
		(uint8_t)(d | 0x0C),   // E=1, RS=0, 백라이트=1
		(uint8_t)(d | 0x08)    // E=0 -> 하강 에지에서 LCD 가 D7~D4 를 읽는다
	};
	HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, buf, 2, 100);
}

static void lcd_init_4bit(I2C_LCD_HandleTypeDef *lcd)
{
	HAL_Delay(50);                 // 전원 투입 후 40ms 이상 대기
	lcd_write_nibble(lcd, 0x3);    // Function set (8비트) 1회
	HAL_Delay(5);                  // 4.1ms 이상
	lcd_write_nibble(lcd, 0x3);    // 2회
	HAL_Delay(1);                  // 100us 이상
	lcd_write_nibble(lcd, 0x3);    // 3회
	HAL_Delay(1);
	lcd_write_nibble(lcd, 0x2);    // 4비트 모드로 전환 (여기부터 명령은 니블 2개)
	HAL_Delay(1);

	lcd_send_cmd(lcd, 0x28);       // Function set: 4비트, 2줄, 5x8
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x08);       // 표시 OFF
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x01);       // 화면 지우기 (1.52ms)
	HAL_Delay(2);
	lcd_send_cmd(lcd, 0x06);       // Entry mode: 커서 오른쪽 이동
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x0C);       // 표시 ON, 커서 OFF, 깜빡임 OFF
	HAL_Delay(1);
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);
  // I2C 스캐너 — 실제 주소 확인용
	printf("I2C Scan Start\n");
	for (uint8_t addr = 0x00; addr < 0xF0; addr += 2) {
		if (HAL_I2C_IsDeviceReady(&hi2c1, addr, 1, 10) == HAL_OK) {
			printf("Found: 0x%02X (7bit: 0x%02X)\n", addr, addr >> 1);
		}
	}
	printf("I2C Scan Done\n");

  // 2. 핸들에 하드웨어 레일 바인딩
  lcd1.hi2c    = &hi2c1;
  lcd1.address = 0x4E;   // PCF8574 기본 주소: 0x27<<1 = 0x4E
  
  // 3. LCD 초기화 (백라이트 자동 ON 포함)
  //    라이브러리 lcd_init() 대신 니블 정렬을 맞춘 lcd_init_4bit() 를 쓴다 (USER CODE 0 설명 참고)
  lcd_init_4bit(&lcd1);

  // 4. 디스플레이 클리어
  lcd_clear(&lcd1);
  printf("LCD clear\n");

  // 5. 1행 출력: gotoxy(col, row) → puts()
  lcd_gotoxy(&lcd1, 0, 0);
  lcd_puts(&lcd1, "STM32 with C Code");
  printf("STM32 with C Code\n");

  // 6. 2행 출력
  lcd_gotoxy(&lcd1, 0, 1);
  lcd_puts(&lcd1, "No OOP Paradigm");
  printf("No OOP Paradigm\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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
  while (1)
  {
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

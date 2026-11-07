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
 * 02_PCF8574_Output  —  [계층: HAL]  I2C I/O 확장기 PCF8574 의 8비트 출력 시험
 *
 * 목적
 *   PCF8574 는 I2C 로 받은 1바이트를 P7~P0 핀에 그대로 내보내는 8비트 확장기이다.
 *   이 예제는 LCD 같은 응용 없이 "바이트 하나 쓰기 = 핀 8개 상태" 관계만 확인한다.
 *   다음 예제 03_PCF8574_LCD 는 같은 원리로 문자 LCD(HD44780)를 구동한다.
 *
 * 구성
 *   I2C1 (100kHz, PB8=SCL / PB9=SDA 로 리맵) -> PCF8574 (A2=A1=A0=1, 7비트 주소 0x27)
 *   P0~P7 에 LED 8개: VCC - 저항(330Ω~1kΩ) - LED 애노드 - LED 캐소드 - Px  (액티브 Low)
 *
 * PCF8574 핀 구조 (준양방향, quasi-bidirectional)
 *   비트 = 0 : 강한 Low (전류를 흡수, 수 mA 이상) -> LED 켜짐
 *   비트 = 1 : 약한 풀업(약 100uA)                -> LED 꺼짐. 이 상태의 핀은 입력으로도 쓸 수 있다.
 *   즉 PCF8574 는 전류를 "내보내기"보다 "흡수"를 잘하므로 LED 는 Low 에서 켜지게 연결한다.
 *
 * I2C 프레임
 *   쓰기: START | 0x4E(0x27<<1 | W) | ACK | 데이터 | ACK | STOP  -> 데이터의 ACK 직후 핀이 바뀐다
 *   읽기: START | 0x4F(0x27<<1 | R) | ACK | 핀 상태 | NACK | STOP -> 실제 핀 전압을 읽는다
 *   HAL 은 8비트 형태 주소(0x4E)를 받고 R/W 비트는 함수가 붙인다.
 *
 * 하드웨어 대응 (RM0008)
 *   AFIO_MAPR.I2C1_REMAP = 1 : I2C1 을 PB6/PB7 에서 PB8/PB9 로 이동 (HAL_msp 에서 설정)
 *   I2C1_CR2.FREQ=32, CCR=160, TRISE=33 : PCLK1 32MHz 기준 100kHz 표준 모드
 *
 * I2C 는 폴링 방식의 블로킹 전송이다 (I2C 인터럽트/콜백 없음).
 * 활성화된 ISR (stm32f1xx_it.c)
 *   SysTick_Handler      : 1ms 틱 (HAL_IncTick, HAL_Delay/타임아웃용)
 *   EXTI15_10_IRQHandler : B1(PC13) (CubeMX 기본 설정). HAL_GPIO_EXTI_Callback 을 재정의하지 않으므로
 *                          weak 빈 함수가 실행되고 아무 일도 없다.
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PCF_ADDR   (0x27 << 1)   // HAL 주소 인자 = 7비트 주소 0x27 을 1비트 왼쪽 시프트 = 0x4E
#define STEP_MS    300           // 패턴 하나를 보여 주는 시간 [ms]
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* 출력 패턴 표 (액티브 Low: 0 인 비트의 LED 가 켜진다)
 *   0xFF : 모두 끔 (전원 투입 직후 PCF8574 의 기본 상태와 같다)
 *   0x00 : 모두 켬
 *   0xFE, 0xFD, ... 0x7F : LED 하나가 P0 -> P7 로 이동
 *   0xAA, 0x55 : 하나 건너 하나 */
static const uint8_t patterns[] = {
  0xFF, 0x00,
  0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F,
  0xAA, 0x55
};
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
// printf -> UART2 (PA2) 리다이렉션
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

/* PCF8574 에 1바이트 쓰기: 비트 n 이 핀 Pn 의 출력이 된다.
 * HAL_I2C_Master_Transmit 이 I2C1_CR1.START -> DR(주소) -> SR1.ADDR -> DR(데이터) -> SR1.BTF -> CR1.STOP
 * 순서를 폴링으로 처리한다. 주소 NACK(SR1.AF) 이면 HAL_ERROR 를 돌려준다. */
static HAL_StatusTypeDef pcf_write(uint8_t value)
{
	return HAL_I2C_Master_Transmit(&hi2c1, PCF_ADDR, &value, 1, 10);
}

/* PCF8574 에서 1바이트 읽기: 핀 P7~P0 의 실제 전압 레벨을 돌려준다(출력 래치 값이 아니다). */
static HAL_StatusTypeDef pcf_read(uint8_t *value)
{
	return HAL_I2C_Master_Receive(&hi2c1, PCF_ADDR, value, 1, 10);
}

/* 한 줄 출력 예: W=0xFE R=0xFE  P7..P0=11111110  LED=-------*
 * LED 표시는 P7 이 왼쪽, P0 이 오른쪽이며 '*' 가 켜진 LED(비트 0)이다. */
static void print_state(uint8_t w, uint8_t r)
{
	char bits[9], led[9];
	for (int i = 0; i < 8; i++) {
		uint8_t b = (w >> (7 - i)) & 1u;
		bits[i] = b ? '1' : '0';
		led[i]  = b ? '-' : '*';
	}
	bits[8] = led[8] = '\0';
	printf("W=0x%02X R=0x%02X  P7..P0=%s  LED=%s%s\n",
	       w, r, bits, led, (w == r) ? "" : "  <- R differs (pin pulled low externally?)");
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
  HAL_Delay(100);   // PCF8574 전원 안정 대기
  printf("\n01_PCF8574_Output: PCF8574 @ 0x%02X (7bit 0x%02X)\n", PCF_ADDR, PCF_ADDR >> 1);

  // 1. 장치 확인: 주소만 보내고 ACK 를 기다린다 (SR1.ADDR=1 이면 ACK, SR1.AF=1 이면 NACK)
  if (HAL_I2C_IsDeviceReady(&hi2c1, PCF_ADDR, 3, 10) != HAL_OK) {
    printf("NACK: no response. Check wiring (PB8=SCL, PB9=SDA), pull-ups, A2..A0 pins\n");
  } else {
    printf("ACK: PCF8574 found\n");
  }

  // 2. 시작 상태: 모두 끔(0xFF). 전원 투입 직후 PCF8574 의 출력도 0xFF 이다.
  pcf_write(0xFF);
  uint32_t idx = 0;   // patterns[] 위치
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 3. 패턴 하나 쓰기 -> 다시 읽어 비교 -> UART 출력
    uint8_t w = patterns[idx];
    uint8_t r = 0;
    if (pcf_write(w) != HAL_OK) {
      printf("write NACK/timeout (err=0x%08lX)\n", (unsigned long) HAL_I2C_GetError(&hi2c1));
    } else if (pcf_read(&r) != HAL_OK) {
      printf("read NACK/timeout (err=0x%08lX)\n", (unsigned long) HAL_I2C_GetError(&hi2c1));
    } else {
      print_state(w, r);
    }

    idx = (idx + 1) % (sizeof patterns / sizeof patterns[0]);
    if (idx == 0) printf("----\n");   // 한 바퀴 끝

    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);   // 살아 있음 표시 (PA5)
    HAL_Delay(STEP_MS);
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
  // HAL_I2C_Init 결과 (PCLK1 = 32MHz): CR2.FREQ = 32, CCR = 160 (F/S=0, 100kHz), TRISE = 33
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

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
 * 03_I2C_MPU6050_HAL_c  —  [계층: HAL]  I2C 레지스터 읽기로 MPU6050 가속도/자이로/온도 읽기
 *
 * 배선: MPU6050(GY-521) VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND (7비트 주소 0x68)
 *
 * 동작
 *   1) WHO_AM_I(0x75) 를 읽어 0x68 인지 확인
 *   2) PWR_MGMT_1(0x6B)=0 으로 슬립 해제 (리셋 직후 MPU6050 은 슬립 상태)
 *   3) 200ms 마다 0x3B 부터 14바이트(ACCEL XYZ, TEMP, GYRO XYZ)를 한 번에 읽어 UART(115200)로 출력
 *
 * 단위 변환 (기본 범위: 가속도 ±2g, 자이로 ±250°/s)
 *   가속도: 16384 LSB/g  -> mg = raw x 1000 / 16384
 *   자이로: 131 LSB/(°/s) -> 0.1°/s = raw x 10 / 131
 *   온도  : T[°C] = raw/340 + 36.53  -> 0.01°C 단위 = raw x 100 / 340 + 3653   (부동소수점 없이 정수 연산)
 *
 * I2C 메모리 읽기 시퀀스 (HAL_I2C_Mem_Read)
 *   START - [0x68<<1|W] - [레지스터 주소] - RESTART - [0x68<<1|R] - 데이터 N바이트(마지막 NACK) - STOP
 *
 * 인터럽트를 사용하지 않는다 (폴링). 비동기로 바꾸려면 HAL_I2C_Mem_Read_IT/_DMA 와
 * HAL_I2C_MemRxCpltCallback 을 사용한다 (ISR: I2C1_EV_IRQHandler/I2C1_ER_IRQHandler -> HAL_I2C_EV/ER_IRQHandler).
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
#define MPU_ADDR      (0x68 << 1)   /* HAL 은 7비트 주소를 왼쪽으로 1비트 민 값을 요구 */
#define REG_WHO_AM_I  0x75
#define REG_PWR_MGMT1 0x6B
#define REG_ACCEL_OUT 0x3B

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
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
    char msg[128];
    uint8_t who = 0;
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
    if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)
    {
        print("\r\nMPU6050 not responding (check wiring / pull-ups / AD0)\r\n");
        Error_Handler();
    }
    snprintf(msg, sizeof msg, "\r\nWHO_AM_I = 0x%02X (expect 0x68)\r\n", who);
    print(msg);

    uint8_t wake = 0x00;                                   /* SLEEP=0, 내부 8MHz 클럭 */
    if (HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, REG_PWR_MGMT1, I2C_MEMADD_SIZE_8BIT, &wake, 1, 100) != HAL_OK) Error_Handler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        uint8_t b[14];
        if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_ACCEL_OUT, I2C_MEMADD_SIZE_8BIT, b, 14, 100) == HAL_OK)
        {
            /* 상위 바이트 먼저(big-endian) 로 도착 -> int16_t 로 조립 */
            int16_t ax = (int16_t)((b[0] << 8) | b[1]);
            int16_t ay = (int16_t)((b[2] << 8) | b[3]);
            int16_t az = (int16_t)((b[4] << 8) | b[5]);
            int16_t tp = (int16_t)((b[6] << 8) | b[7]);
            int16_t gx = (int16_t)((b[8] << 8) | b[9]);
            int16_t gy = (int16_t)((b[10] << 8) | b[11]);
            int16_t gz = (int16_t)((b[12] << 8) | b[13]);

            int32_t t100 = (int32_t)tp * 100 / 340 + 3653;   /* 0.01 °C */
            snprintf(msg, sizeof msg,
                     "acc[mg] %6ld %6ld %6ld | gyro[0.1dps] %6ld %6ld %6ld | temp %ld.%02ld C\r\n",
                     (long)ax * 1000 / 16384, (long)ay * 1000 / 16384, (long)az * 1000 / 16384,
                     (long)gx * 10 / 131, (long)gy * 10 / 131, (long)gz * 10 / 131,
                     (long)(t100 / 100), (long)(t100 % 100));
            print(msg);
        }
        else
        {
            print("read error\r\n");
            HAL_I2C_DeInit(&hi2c1);                        /* 버스 오류 복구: 재초기화 */
            MX_I2C1_Init();
        }
        HAL_Delay(200);
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

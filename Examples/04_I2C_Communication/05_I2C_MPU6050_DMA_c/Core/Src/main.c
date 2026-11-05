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
 * 05_I2C_MPU6050_DMA_c  —  [계층: HAL]  DMA 로 하는 논블로킹 I2C: 전송 중에도 CPU 는 다른 일을 한다
 *
 * 04_I2C_MPU6050 은 HAL_I2C_Mem_Read() 가 전송이 끝날 때까지 CPU 를 붙잡는다.
 *   (버스에 17바이트 x 9클럭 = 153 SCL 클럭 + START/RESTART/STOP -> 약 1.6ms @100kHz)
 * 이 예제는 HAL_I2C_Mem_Read_DMA() 로 읽기를 "시작"만 하고 곧바로 다른 일을 하다가, 완료 콜백에서 결과를 받는다.
 *
 * 주의 (STM32CubeF1 HAL V1.8.x 의 HAL_I2C_Mem_Read_DMA 동작)
 *   - START - [0xD0] - [0x3B] - RESTART - [0xD1] 까지의 "주소 단계"는 함수 안에서 SR1.SB/ADDR/TxE 를
 *     폴링하며 처리한다 (약 0.3ms 동안 CPU 점유, 인터럽트 아님).
 *   - 그 뒤 데이터 14바이트만 DMA1 채널7 이 I2C1->DR 에서 dma_buf 로 옮긴다 (약 1.3ms, CPU 자유).
 *
 * 배선: MPU6050(GY-521) VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND (주소 0x68) — 04_I2C_MPU6050 과 동일
 *
 * 동작
 *   - 100ms 마다 0x3B 부터 14바이트를 DMA 로 읽기 시작. 그동안 main 은 "다른 일"(loops 카운터)을 계속 수행.
 *   - 완료 콜백이 raw 데이터를 복사하고 플래그를 세움 -> main 이 변환해 UART(115200)로 출력.
 *   - 출력에는 "전송 시작~완료 사이에 main 이 돈 루프 수"가 함께 표시되어 CPU 가 자유로웠음을 보여준다.
 *   - 오류 복구 (main 이 I2C 를 재초기화):
 *       주소 단계 NACK(SR1.AF)/타임아웃 -> HAL_I2C_Mem_Read_DMA() 가 HAL_ERROR/HAL_BUSY 를 반환 (ErrorCallback 은 불리지 않음)
 *       데이터 단계 BERR/ARLO/OVR, DMA 전송 오류(TE) -> HAL_I2C_ErrorCallback()
 *       완료도 오류도 오지 않음 -> main 의 소프트웨어 타임아웃(XFER_TIMEOUT_MS)
 *
 * DMA 채널 매핑 (RM0008 13.3.7, 표 78): I2C1_TX -> DMA1 채널6,  I2C1_RX -> DMA1 채널7
 *   I2C1->CR2.DMAEN=1, LAST=1 (LAST: 다음 DMA EOT 가 마지막 -> 마지막 수신 바이트에 NACK 자동)
 *   STOP 은 하드웨어가 자동으로 내지 않는다. DMA TC 처리(HAL 내부 I2C_DMAXferCplt)에서 HAL 이 CR1.STOP=1 로 생성한다.
 *
 * ISR vs 콜백
 *   진짜 ISR (stm32f1xx_it.c): DMA1_Channel7_IRQHandler (I2C1_RX, 이 예제의 핵심), I2C1_ER_IRQHandler (오류)
 *     -> HAL_DMA_IRQHandler / HAL_I2C_ER_IRQHandler
 *       -> HAL 콜백 HAL_I2C_MemRxCpltCallback() (수신 완료), HAL_I2C_ErrorCallback() (오류)  (이 파일, ISR 문맥)
 *   DMA1_Channel6_IRQHandler(I2C1_TX), I2C1_EV_IRQHandler 는 CubeMX 가 함께 켜 두었지만
 *   Mem_Read_DMA 는 TX DMA 와 CR2.ITEVTEN 을 쓰지 않으므로 이 예제 흐름에서는 호출되지 않는다.
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
#define MPU_ADDR      (0x68 << 1)
#define REG_WHO_AM_I  0x75
#define REG_PWR_MGMT1 0x6B
#define REG_ACCEL_OUT 0x3B
#define XFER_TIMEOUT_MS 20   /* DMA 읽기 1회는 약 1.6ms. 이 시간 안에 완료/오류 콜백이 없으면 멈춘 것으로 본다 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t dma_buf[14];                     /* DMA 가 채우는 버퍼 */
static uint8_t data[14];                        /* 콜백이 복사해 둔 사본 */
static volatile uint8_t rx_done, rx_error;
static volatile uint32_t loops_during;          /* 전송 중 main 이 돈 횟수 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
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
    uint8_t who = 0, wake = 0;
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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
    if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)   /* 초기화는 블로킹 API 로 */
    {
        print("\r\nMPU6050 not responding\r\n");
        Error_Handler();
    }
    /* PWR_MGMT_1(0x6B) = 0x00: SLEEP 비트(bit6) 해제, 내부 8MHz 발진기 사용. 리셋값은 0x40(슬립) */
    if (HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, REG_PWR_MGMT1, I2C_MEMADD_SIZE_8BIT, &wake, 1, 100) != HAL_OK)
        print("\r\nPWR_MGMT_1 write failed (sensor may stay asleep)\r\n");
    snprintf(msg, sizeof msg, "\r\nWHO_AM_I=0x%02X, starting DMA reads\r\n", who);
    print(msg);

    uint32_t t_read = HAL_GetTick();
    uint32_t t_start = 0;                                     /* DMA 읽기를 시작한 시각 (타임아웃 판정용) */
    uint32_t loops = 0;
    uint8_t busy = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        loops++;                                               /* "다른 일" 흉내 */

        if (!busy && HAL_GetTick() - t_read >= 100)
        {
            t_read += 100;
            rx_done = 0; rx_error = 0; loops = 0;
            /* 주소 단계(약 0.3ms)는 이 함수 안에서 폴링, 데이터 14바이트는 DMA1 채널7 이 옮긴다 */
            HAL_StatusTypeDef st = HAL_I2C_Mem_Read_DMA(&hi2c1, MPU_ADDR, REG_ACCEL_OUT, I2C_MEMADD_SIZE_8BIT, dma_buf, 14);
            if (st == HAL_OK)
            {
                busy = 1;                                      /* 데이터 단계는 DMA 에 맡기고 즉시 반환 */
                t_start = HAL_GetTick();
            }
            else
            {
                /* 주소 단계 NACK(SR1.AF), SB/ADDR 타임아웃, SR2.BUSY 고착은 ErrorCallback 없이 여기로 온다 */
                snprintf(msg, sizeof msg, "I2C start failed (st=%d, err=0x%02lX) -> re-init\r\n",
                         (int)st, (unsigned long)HAL_I2C_GetError(&hi2c1));
                print(msg);
                HAL_I2C_DeInit(&hi2c1);
                MX_I2C1_Init();
            }
        }

        if (busy && rx_done)
        {
            busy = 0;
            loops_during = loops;
            int16_t ax = (int16_t)((data[0] << 8) | data[1]);
            int16_t ay = (int16_t)((data[2] << 8) | data[3]);
            int16_t az = (int16_t)((data[4] << 8) | data[5]);
            int16_t gz = (int16_t)((data[12] << 8) | data[13]);
            snprintf(msg, sizeof msg, "acc[mg] %6ld %6ld %6ld | gyroZ[0.1dps] %6ld | main loops during DMA = %lu\r\n",
                     (long)ax * 1000 / 16384, (long)ay * 1000 / 16384, (long)az * 1000 / 16384,
                     (long)gz * 10 / 131, (unsigned long)loops_during);
            print(msg);
        }
        else if (busy && (rx_error || HAL_GetTick() - t_start > XFER_TIMEOUT_MS))
        {
            /* rx_error: ErrorCallback(BERR/ARLO/OVR/DMA TE). BERR 이면 HAL 이 CR1.SWRST=1 을 남겨 두므로 재초기화가 필수.
             * 시간 초과: 콜백이 오지 않음 (IT/DMA 방식 HAL 은 자체 타임아웃이 없다) */
            busy = 0;
            snprintf(msg, sizeof msg, "I2C %s (err=0x%02lX) -> re-init\r\n",
                     rx_error ? "error" : "timeout", (unsigned long)HAL_I2C_GetError(&hi2c1));
            print(msg);
            HAL_I2C_DeInit(&hi2c1);                            /* CR1.PE=0, DMA 채널 해제, 클럭/GPIO 해제 (MspDeInit) */
            MX_I2C1_Init();                                    /* CR1.SWRST 로 리셋 후 FREQ/CCR/TRISE 재설정 */
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
  /* HAL_I2C_Init -> (MspInit) 후 CR1.SWRST 로 리셋, I2C1->CR2.FREQ = PCLK1(MHz) = 32,
   * CCR: F/S=0(표준 모드), CCR = 32MHz / (2 x 100kHz) = 160,  TRISE = FREQ + 1 = 33 (최대 상승 시간 1000ns),
   * OAR1.ADD=0 (bit14 는 HAL 이 1 로 둔다), CR1.PE = 1. NoStretch/GeneralCall/DualAddress = CR1.NOSTRETCH/ENGC, OAR2.ENDUAL 비트 */
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

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
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥)                                                          */
/* ------------------------------------------------------------------------- */
/* [HAL 콜백] DMA1_Channel7_IRQHandler(ISR) -> HAL_DMA_IRQHandler(DMA1 TCIF7) -> I2C_DMAXferCplt
 * (CR1.STOP=1, CR2.LAST=0, CR2.DMAEN=0) -> 여기. dma_buf 를 data 로 복사해 두고 main 에 알린다 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) { memcpy(data, dma_buf, sizeof data); rx_done = 1; }
}

/* [HAL 콜백] I2C1_ER_IRQHandler(SR1.BERR/ARLO/OVR) 또는 DMA 전송 오류(DMA1 TEIF7) -> 여기.
 * 재초기화는 ISR 문맥에서 하지 않고 플래그만 세워 main 이 처리한다 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) rx_error = 1;
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

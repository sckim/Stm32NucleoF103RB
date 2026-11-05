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
 * 03_I2C_Slave_Loopback_c  —  [계층: HAL]  I2C 슬레이브 모드: 보드 한 장 안에서 I2C1(마스터) <-> I2C2(슬레이브) 시험
 *
 * 배선 (점퍼선 2개 + 풀업)
 *   PB6 (I2C1_SCL) ---- PB10 (I2C2_SCL)
 *   PB7 (I2C1_SDA) ---- PB11 (I2C2_SDA)
 *   SCL, SDA 각각 3.3V 로 4.7kΩ 풀업 권장 (내부 풀업만으로는 100kHz 에서 신호가 둔해질 수 있다)
 *
 * 역할
 *   I2C1 = 마스터,  I2C2 = 슬레이브 (7비트 주소 0x32). 슬레이브는 16바이트 "레지스터 파일"을 가진 가상 센서처럼 동작한다.
 *   전송 규약(모든 프레임 길이를 고정해 HAL 순차 API 로 단순하게 처리):
 *     쓰기  : 마스터가 2바이트 [reg, value] 송신          -> regs[reg] = value
 *     읽기  : 마스터가 2바이트 [0x80|reg, 0] 송신(포인터 설정) -> 이어서 4바이트 수신 = regs[reg .. reg+3]
 *
 * 이 예제가 보여주는 것
 *   - HAL_I2C_EnableListen_IT(): 슬레이브가 자기 주소 호출을 기다리는 상태(리슨)
 *   - 주소 일치 콜백(HAL_I2C_AddrCallback)에서 방향(TransferDirection)에 따라 수신/송신을 시작하는 슬레이브 순차 API
 *   - 전송이 끝나면 다시 리슨으로 복귀 (ListenCplt 콜백) — 슬레이브가 항상 응답 가능한 상태를 유지하는 구조
 *   결과: 마스터가 쓴 값을 읽어 되돌려 받아 일치하는지 검증하고 UART(115200)로 PASS/FAIL 출력.
 *
 * 하드웨어 대응
 *   I2C2->OAR1 : 자기 주소(0x32<<1),  CR1.ACK/ENGC,  SR1.ADDR(주소 일치)/RXNE/TXE/STOPF/AF,  CR2.ITEVTEN/ITERREN/ITBUFEN
 *   슬레이브는 클럭 스트레칭(SCL 을 잡고 대기)으로 처리 시간을 확보한다 (NoStretchMode = DISABLE)
 *
 * ISR vs 콜백
 *   I2C2_EV_IRQHandler / I2C2_ER_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_I2C_EV_IRQHandler / HAL_I2C_ER_IRQHandler
 *     -> HAL_I2C_AddrCallback / HAL_I2C_SlaveRxCpltCallback / HAL_I2C_SlaveTxCpltCallback / HAL_I2C_ListenCpltCallback / ErrorCallback (이 파일)
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
#define SLAVE_ADDR (0x32 << 1)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* hi2c1 = 마스터, hi2c2 = 슬레이브 */
static uint8_t regs[16];               /* 슬레이브의 가상 레지스터 파일 */
static uint8_t rx_frame[2];            /* 슬레이브가 받은 2바이트 [reg, value] */
static uint8_t ptr;                    /* 읽기 포인터 */
static volatile uint32_t slave_writes, slave_reads;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_I2C_EnableListen_IT(&hi2c2) != HAL_OK) Error_Handler();    /* 슬레이브: 주소 호출 대기 시작 */

  char msg[112];
  uint32_t pass = 0, fail = 0;
  uint32_t round = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t reg = (uint8_t)(round % 12);                           /* reg .. reg+3 이 배열 안에 있도록 */
    uint8_t val = (uint8_t)(0xA0 + (round & 0x0F));

    /* 쓰기: [reg, val] */
    uint8_t wr[2] = { reg, val };
    HAL_StatusTypeDef s1 = HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDR, wr, 2, 50);

    /* 읽기: [0x80|reg, 0] 로 포인터 설정 후 4바이트 수신 */
    uint8_t setp[2] = { (uint8_t)(0x80 | reg), 0 };
    uint8_t got[4] = {0};
    HAL_StatusTypeDef s2 = HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDR, setp, 2, 50);
    HAL_StatusTypeDef s3 = HAL_I2C_Master_Receive(&hi2c1, SLAVE_ADDR, got, 4, 50);

    int ok = (s1 == HAL_OK && s2 == HAL_OK && s3 == HAL_OK && got[0] == val);
    if (ok) pass++; else fail++;

    int n = snprintf(msg, sizeof msg, "wrote regs[%u]=0x%02X, read back 0x%02X 0x%02X 0x%02X 0x%02X  %s  (slave writes=%lu reads=%lu, pass=%lu fail=%lu)\r\n",
                     reg, val, got[0], got[1], got[2], got[3], ok ? "PASS" : "FAIL",
                     (unsigned long)slave_writes, (unsigned long)slave_reads, (unsigned long)pass, (unsigned long)fail);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    HAL_Delay(500);
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
  /* I2C1 = 마스터 (자기 주소 없음). 100kHz 표준 모드: I2C1->CR2.FREQ = 32(APB1 MHz), CCR/TRISE 는 HAL 이 계산 */
  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */


  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */


  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 100;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */
  /* I2C2 = 슬레이브. OwnAddress1 = 100 = SLAVE_ADDR(0x32 << 1)  -> I2C2->OAR1 (7비트 주소는 [7:1])
   * 클럭 스트레칭 사용 (CR1.NOSTRETCH = 0). 이벤트/오류 인터럽트는 HAL_I2C_MspInit 에서 NVIC 에 등록(I2C2_EV/ER). */
  /* USER CODE END I2C2_Init 2 */

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
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): I2C2 = 슬레이브                                          */
/* ------------------------------------------------------------------------- */
/* 마스터가 우리 주소를 불렀다. TransferDirection 은 "마스터 기준"이다:
   I2C_DIRECTION_TRANSMIT = 마스터가 쓴다(슬레이브는 수신), I2C_DIRECTION_RECEIVE = 마스터가 읽는다(슬레이브는 송신) */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    (void)AddrMatchCode;
    if (hi2c->Instance != I2C2) return;
    if (TransferDirection == I2C_DIRECTION_TRANSMIT)
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, rx_frame, 2, I2C_FIRST_AND_LAST_FRAME);
    else
    {
        HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &regs[ptr & 0x0F], 4, I2C_FIRST_AND_LAST_FRAME);
        slave_reads++;
    }
}

/* 슬레이브 수신 완료: 2바이트 [reg, value] 처리 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C2) return;
    if (rx_frame[0] & 0x80) ptr = rx_frame[0] & 0x7F;                 /* 읽기용 포인터 설정 */
    else { regs[rx_frame[0] & 0x0F] = rx_frame[1]; slave_writes++; }  /* 레지스터 쓰기 */
}

/* 주소 호출 한 번의 전송이 모두 끝나면(STOP 또는 NACK) 다시 리슨으로 복귀해야 다음 호출에 응답한다 */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) HAL_I2C_EnableListen_IT(hi2c);
}

/* 슬레이브 송신에서 마스터가 마지막 바이트에 NACK 하는 것은 정상 종료 (AF). 그 외 오류도 리슨 복귀로 복구 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2)
    {
        if (HAL_I2C_GetError(hi2c) != HAL_I2C_ERROR_AF) { /* 필요 시 여기서 오류 통계 증가 */ }
        HAL_I2C_EnableListen_IT(hi2c);
    }
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

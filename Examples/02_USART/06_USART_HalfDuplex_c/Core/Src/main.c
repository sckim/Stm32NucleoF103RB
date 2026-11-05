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
 * 06_USART_HalfDuplex_c  —  [계층: HAL]  USART 단선(Half-duplex) 모드: 한 가닥 선으로 송수신 (RS-485/스마트 서보 방식)
 *
 * 단선 모드란 (RM0008 27장 USART, "Single-wire half-duplex communication")
 *   TX 핀 하나가 송신과 수신을 번갈아 쓴다. 내부적으로 RX 를 TX 핀에 연결하고 CR3.HDSEL=1 로 켠다.
 *   버스에는 오픈드레인 + 풀업이 필요하고, "지금 누가 말하는가"를 프로토콜로 정해야 한다(마스터가 묻고 슬레이브가 답하는 방식이 흔함).
 *   HAL_HalfDuplex_EnableTransmitter()/EnableReceiver() 로 방향을 바꾼다 (CR1.TE / CR1.RE 전환).
 *
 * 이 예제는 보드 한 장으로 시험하도록 구성했다
 *   [마스터] USART1 (단선 모드, PA9)                         [가짜 슬레이브] USART3 (일반 전이중, PB10=TX, PB11=RX)
 *   버스 선 하나:  PA9 ----+---- PB11 (USART3_RX)
 *                          +---- PB10 (USART3_TX)     -> 세 핀을 점퍼선으로 모두 연결하고 3.3V 풀업(4.7kΩ) 권장
 *   동작: 마스터가 "PING" 송신 -> 수신 모드로 전환 -> 슬레이브가 "PONG" 으로 응답 -> 마스터가 수신 확인.
 *         (슬레이브 USART3 는 자기가 보낸 데이터를 RX 로 다시 듣게 되므로 그 4바이트는 버린다)
 *   결과는 ST-Link 가상 COM(USART2, 115200)으로 1초마다 출력.
 *
 * 하드웨어 대응
 *   USART1->CR3.HDSEL(bit3) = 1 : 단선 반이중,  PA9 는 대체기능 오픈드레인(CNF=11).
 *     GPIO_PULLUP 은 F1 의 대체기능 출력 모드에서 적용되지 않으므로 버스에는 외부 풀업(4.7kΩ)을 단다.
 *   송신 후에는 SR.TC(전송 완료)를 확인한 다음 수신으로 바꿔야 마지막 바이트가 잘리지 않는다 (HAL_UART_Transmit 이 TC 까지 기다림)
 *
 * 수신은 "상대가 보내기 전에" 미리 걸어 둔다 (HAL_UART_Receive_IT, CR1.RXNEIE=1)
 *   USART 수신 데이터 레지스터(DR)는 1바이트뿐이다. 송신을 블로킹으로 다 끝낸 뒤에 HAL_UART_Receive() 로 받기 시작하면
 *   그 사이 아무도 DR 을 읽지 않으므로 첫 바이트만 남고 나머지는 오버런(SR.ORE)으로 사라진다.
 *   그래서 슬레이브 수신(USART3)과 마스터 수신(USART1)을 각각 상대 송신 "전에" 인터럽트로 무장하고,
 *   송신은 폴링(HAL_UART_Transmit), 수신 완료는 RxState 가 READY 로 돌아오는지로 확인한다(WaitRx).
 *
 * ISR vs 콜백
 *   USART1_IRQHandler / USART3_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler()
 *     -> RXNE 마다 DR 을 읽어 버퍼에 저장, 지정한 바이트 수가 차면 RXNEIE 를 끄고 RxState = READY.
 *   이 예제는 HAL_UART_RxCpltCallback 을 쓰지 않고 main 이 RxState 를 확인한다.
 *   실제 RS-485 에서는 트랜시버의 DE/RE 핀을 방향 전환 시점에 GPIO 로 제어한다.
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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;    /* 마스터: 단선 모드 */
UART_HandleTypeDef huart2;    /* 로그 출력 (가상 COM) */
UART_HandleTypeDef huart3;    /* 가짜 슬레이브 */

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef WaitRx(UART_HandleTypeDef *huart, uint32_t timeout_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * 인터럽트 수신(HAL_UART_Receive_IT)이 끝날 때까지 기다린다.
 *   - 정상: HAL_UART_IRQHandler 가 지정 바이트 수를 다 받으면 RxState 를 READY 로 돌린다 -> HAL_OK
 *   - 오류: ORE/FE/NE 등이 나면 HAL 이 수신을 중단하고 ErrorCode 를 남긴다 -> HAL_ERROR
 *   - 시간 초과: 수신을 취소(HAL_UART_AbortReceive: RXNEIE=0, RxState=READY)해 다음 시도를 준비한다 -> HAL_TIMEOUT
 */
static HAL_StatusTypeDef WaitRx(UART_HandleTypeDef *huart, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  while (huart->RxState != HAL_UART_STATE_READY)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      HAL_UART_AbortReceive(huart);
      return HAL_TIMEOUT;
    }
  }
  return (huart->ErrorCode == HAL_UART_ERROR_NONE) ? HAL_OK : HAL_ERROR;
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
  uint32_t ok = 0, fail = 0, seq = 0;
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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t req[4] = { 'P', 'I', 'N', 'G' };
    uint8_t rsp[4] = {0};
    uint8_t slave_rx[8] = {0};
    HAL_StatusTypeDef st_slave = HAL_ERROR, st_master = HAL_ERROR;

    /* 1) 슬레이브 수신을 먼저 무장한다: USART3->CR1.RXNEIE=1 (바이트마다 ISR 이 DR 을 읽어 slave_rx 에 저장)
     *    마스터가 송신을 시작한 뒤에 받기 시작하면 DR(1바이트) 에 첫 바이트만 남고 나머지는 ORE 로 사라진다. */
    HAL_UART_Receive_IT(&huart3, slave_rx, 4);

    /* 2) 마스터: 송신 모드로 바꾸고 "PING" 전송 (마지막 바이트 TC 까지 대기) */
    HAL_HalfDuplex_EnableTransmitter(&huart1);                 /* USART1->CR1.TE=1, RE=0 */
    HAL_UART_Transmit(&huart1, req, 4, 50);
    st_slave = WaitRx(&huart3, 20);                            /* 4바이트 = 약 0.7ms (57600bps) */

    if (st_slave == HAL_OK && memcmp(slave_rx, "PING", 4) == 0)
    {
      /* 3) 마스터: 수신 모드로 바꾸고 응답 수신을 먼저 무장한다 (USART1->CR1.TE=0, RE=1, RXNEIE=1) */
      HAL_HalfDuplex_EnableReceiver(&huart1);
      HAL_UART_Receive_IT(&huart1, rsp, 4);

      /* 4) 슬레이브: 자기 송신이 PB11 로 되돌아오므로(같은 선) 에코 4바이트를 받을 수신도 먼저 무장한 뒤 "PONG" 응답 */
      uint8_t pong[4] = { 'P', 'O', 'N', 'G' };
      uint8_t echo[4];
      HAL_UART_Receive_IT(&huart3, echo, 4);
      HAL_UART_Transmit(&huart3, pong, 4, 50);
      WaitRx(&huart3, 20);                                     /* 자기 응답의 에코 제거 */
      st_master = WaitRx(&huart1, 20);                         /* 마스터가 응답 수신 */
    }

    if (st_master == HAL_OK && memcmp(rsp, "PONG", 4) == 0) ok++; else fail++;

    int n = snprintf(msg, sizeof msg, "#%lu  slave got \"%.4s\"  master got \"%.4s\"  ok=%lu fail=%lu\r\n",
                     (unsigned long)seq++, slave_rx, rsp, (unsigned long)ok, (unsigned long)fail);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_HalfDuplex_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* HAL_HalfDuplex_Init: CR3.HDSEL = 1, CR2.LINEN/CLKEN = 0, CR3.SCEN/IREN = 0 */

  /* USER CODE END USART1_Init 2 */

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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 57600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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

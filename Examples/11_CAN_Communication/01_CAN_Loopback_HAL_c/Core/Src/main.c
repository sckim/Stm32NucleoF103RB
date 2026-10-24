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
 * 01_CAN_Loopback_HAL_c  —  [계층: HAL]  bxCAN 루프백 모드 (트랜시버/상대 노드 없이 송수신 시험)
 *
 * 동작
 *   - CAN1 을 500kbps 루프백 모드로 설정. 500ms 마다 표준 ID 0x123, 8바이트(증가하는 카운터) 프레임을 송신.
 *   - 루프백 모드에서는 송신 프레임이 내부에서 수신 경로로 되돌아오므로, FIFO0 수신 인터럽트가 걸린다.
 *   - 수신 콜백이 프레임을 읽어 오면 main 이 송신 내용과 비교해 UART(115200)로 출력.
 *
 * 실제 버스에 연결하려면
 *   1) CAN_MODE_LOOPBACK -> CAN_MODE_NORMAL 로 변경 (CubeMX: CAN > Operating Mode, 또는 MX_CAN_Init 의 hcan.Init.Mode)
 *   2) 3.3V CAN 트랜시버(SN65HVD230 등)를 PA12(CAN_TX), PA11(CAN_RX)에 연결, 버스 양 끝 120Ω 종단
 *   3) 상대 노드도 500kbps 로 설정. Nucleo 보드에는 트랜시버가 없다.
 *   * 주의: PA11/PA12 는 USB 와 같은 핀이고, bxCAN 과 USB 는 512바이트 전용 SRAM 을 공유해 동시에 쓸 수 없다.
 *
 * 비트 타이밍 (RM0008 24장)  CAN 클럭 = APB1 = 32MHz
 *   Prescaler = 4 -> tq = 4/32MHz = 125ns
 *   1 비트 = Sync(1) + BS1(13) + BS2(2) = 16 tq = 2us -> 500kbps
 *   샘플 포인트 = (1+13)/16 = 87.5%,   SJW = 1tq
 *
 * 하드웨어 대응
 *   CAN1->MCR : INRQ(초기화 요청) -> 초기화 모드에서 BTR 설정, CAN1->BTR.LBKM=1 (루프백)
 *   CAN1->FMR/FA1R/FM1R/FS1R/F0R1,F0R2 : 필터 뱅크 0 을 "32비트 마스크 모드, 마스크 0 = 모든 ID 통과"
 *   CAN1->IER.FMPIE0 : FIFO0 메시지 수신 인터럽트,  TSR/TDLxR/TDHxR/TIxR : 송신 메일박스(3개)
 *
 * ISR vs 콜백
 *   USB_LP_CAN1_RX0_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_CAN_IRQHandler()
 *     -> HAL_CAN_RxFifo0MsgPendingCallback() (이 파일) : FIFO0 에 프레임이 도착
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
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint8_t rx_flag;
static CAN_RxHeaderTypeDef rx_hdr;
static uint8_t rx_data[8];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
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
  char msg[120];
  uint8_t tx_data[8] = {0};
  uint32_t seq = 0;
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
  MX_CAN_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 필터 뱅크 0: 32비트 마스크 모드, ID=0 / 마스크=0 -> 모든 프레임을 FIFO0 로 통과 */
  CAN_FilterTypeDef f = {0};
  f.FilterBank = 0;
  f.FilterMode = CAN_FILTERMODE_IDMASK;
  f.FilterScale = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh = 0;  f.FilterIdLow = 0;
  f.FilterMaskIdHigh = 0;  f.FilterMaskIdLow = 0;
  f.FilterFIFOAssignment = CAN_RX_FIFO0;
  f.FilterActivation = ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) Error_Handler();

  if (HAL_CAN_Start(&hcan) != HAL_OK) Error_Handler();                                        /* MCR.INRQ 해제 -> 버스 참여 */
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();   /* IER.FMPIE0 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    CAN_TxHeaderTypeDef h = {0};
    h.StdId = 0x123;
    h.IDE = CAN_ID_STD;                    /* 11비트 표준 ID */
    h.RTR = CAN_RTR_DATA;
    h.DLC = 8;
    h.TransmitGlobalTime = DISABLE;

    memcpy(tx_data, &seq, 4);
    tx_data[4] = 0xDE; tx_data[5] = 0xAD; tx_data[6] = 0xBE; tx_data[7] = 0xEF;

    uint32_t mailbox;
    rx_flag = 0;
    if (HAL_CAN_AddTxMessage(&hcan, &h, tx_data, &mailbox) != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)"TX fail\r\n", 9, 100);
    }
    else
    {
        uint32_t t0 = HAL_GetTick();
        while (!rx_flag && (HAL_GetTick() - t0) < 50) {}      /* 루프백 수신 대기 (최대 50ms) */

        int ok = rx_flag && rx_hdr.StdId == 0x123 && rx_hdr.DLC == 8 && memcmp(rx_data, tx_data, 8) == 0;
        int n = snprintf(msg, sizeof msg, "TX id=0x123 seq=%lu | RX id=0x%03lX dlc=%lu | %s\r\n",
                         (unsigned long)seq, (unsigned long)rx_hdr.StdId, (unsigned long)rx_hdr.DLC,
                         ok ? "MATCH" : "NO/MISMATCH");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
    seq++;
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

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_LOOPBACK;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */
  /* 비트 타이밍 (CAN 클럭 = APB1 = 32MHz): tq = 4/32MHz = 125ns, 1비트 = 1+13+2 = 16tq = 2us -> 500kbps.
   * Mode  -> CAN1->BTR.LBKM/SILM,  AutoBusOff -> CAN1->MCR.ABOM,  AutoRetransmission -> MCR.NART(반전) */
  /* USER CODE END CAN_Init 2 */

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
/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): FIFO0 에 수신 프레임이 생겼을 때                          */
/* ------------------------------------------------------------------------- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_)
{
    if (HAL_CAN_GetRxMessage(hcan_, CAN_RX_FIFO0, &rx_hdr, rx_data) == HAL_OK)   /* RFxR.RFOM 으로 FIFO 해제 */
        rx_flag = 1;
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

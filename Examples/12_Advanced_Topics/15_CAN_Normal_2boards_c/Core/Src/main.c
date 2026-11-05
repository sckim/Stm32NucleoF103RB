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
 * 15_CAN_Normal_2boards_c  —  [계층: HAL]  실제 CAN 버스(Normal 모드): 보드 2장 + 트랜시버, 오류 카운터와 버스오프 복구
 *
 * 01/02 예제는 루프백이라 "ACK 를 주는 상대"가 없었다. 실제 버스에서는 **다른 노드가 ACK 를 해 주어야** 송신이 성공한다.
 * 이 예제는 같은 코드를 보드 두 장에 올리고(노드 번호만 다르게) 서로 주고받는다.
 *
 * 배선 (보드마다 3.3V CAN 트랜시버 필요: SN65HVD230, TJA1051T/3 등)
 *   STM32 PA12 (CAN_TX) -> 트랜시버 TXD,  PA11 (CAN_RX) <- 트랜시버 RXD,  3.3V, GND
 *   트랜시버 CANH-CANH, CANL-CANL 로 두 보드를 연결하고 **버스 양 끝에 120Ω 종단 저항**(CANH-CANL 사이)
 *   Nucleo 에는 트랜시버가 없다. (PA11/PA12 는 USB 와 같은 핀이라 CAN 과 USB 는 동시에 못 쓴다)
 *
 * 빌드: 노드 번호는 platformio.ini 의 build_flags 에 `-DNODE_ID=0` 또는 `-DNODE_ID=1` 로 지정한다 (기본 0).
 *   보드 A 는 0, 보드 B 는 1 로 각각 빌드/업로드. 노드마다 송신 ID = 0x100 + NODE_ID 가 다르다.
 *
 * 동작
 *   - 500ms 마다 프레임 [ID 0x100+NODE_ID, 데이터: 노드번호, 32비트 카운터, LD2 상태] 송신, 수신하면 LD2 토글.
 *   - 수신 프레임을 UART(115200)로 출력하고, 상대 카운터가 건너뛰면(손실) 알린다.
 *   - 1초마다 **CAN 오류 카운터**(TEC 송신 오류 수, REC 수신 오류 수)와 상태를 출력: 상대가 없으면 ACK 오류로 TEC 가 올라가
 *     (Error Active -> Error Passive TEC > 127 -> Bus-Off TEC > 255) 순서로 나빠지는 것을 볼 수 있다.
 *   - Bus-Off 는 `AutoBusOff`(하드웨어 자동 복구, 128 x 11 recessive 비트 뒤)로 회복. 송신 메일박스가 막히면(ACK 없음) 요청을 취소해 재시도.
 *
 * 필터: 모든 프레임 통과 (필터 설정은 02_CAN_Filter 참고). 비트 타이밍: 500kbps (01_CAN_Loopback 참고).
 *
 * ISR vs 콜백
 *   USB_LP_CAN1_RX0_IRQHandler(수신) / CAN1_SCE_IRQHandler(상태 변화·오류) (stm32f1xx_it.c, 진짜 ISR) -> HAL_CAN_IRQHandler()
 *     -> HAL_CAN_RxFifo0MsgPendingCallback() / HAL_CAN_ErrorCallback() (이 파일)
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
#ifndef NODE_ID
#define NODE_ID 0        /* platformio.ini build_flags 의 -DNODE_ID=0/1 로 덮어쓴다 */
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint8_t rx_flag, err_flag;
static volatile uint32_t err_code;
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
  char msg[128];
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
  CAN_FilterTypeDef f = {0};                                            /* 모든 ID 통과 */
  f.FilterBank = 0;
  f.FilterMode = CAN_FILTERMODE_IDMASK;
  f.FilterScale = CAN_FILTERSCALE_32BIT;
  f.FilterFIFOAssignment = CAN_RX_FIFO0;
  f.FilterActivation = ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) Error_Handler();
  if (HAL_CAN_Start(&hcan) != HAL_OK) Error_Handler();
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE |
                                      CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE | CAN_IT_ERROR);

  snprintf(msg, sizeof msg, "\r\n[CAN normal mode] node %d, tx id 0x%03X, 500kbps\r\n", NODE_ID, 0x100 + NODE_ID);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100);

  uint32_t counter = 0, t_tx = HAL_GetTick(), t_st = HAL_GetTick();
  uint32_t last_peer_cnt = 0;
  int have_peer = 0;
  uint32_t tx_skipped = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GetTick() - t_tx >= 500)
    {
        t_tx += 500;
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)              /* 이전 프레임이 ACK 를 못 받아 메일박스가 막힘 */
        {
            HAL_CAN_AbortTxRequest(&hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            tx_skipped++;
        }
        CAN_TxHeaderTypeDef h = {0};
        h.StdId = 0x100u + NODE_ID;
        h.IDE = CAN_ID_STD;  h.RTR = CAN_RTR_DATA;  h.DLC = 6;
        h.TransmitGlobalTime = DISABLE;
        uint8_t d[6] = { (uint8_t)NODE_ID, (uint8_t)counter, (uint8_t)(counter >> 8), (uint8_t)(counter >> 16),
                         (uint8_t)(counter >> 24), (uint8_t)HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin) };
        uint32_t mb;
        if (HAL_CAN_AddTxMessage(&hcan, &h, d, &mb) == HAL_OK) counter++;
    }

    if (rx_flag)
    {
        rx_flag = 0;
        uint32_t peer_cnt = rx_data[1] | (rx_data[2] << 8) | (rx_data[3] << 16) | ((uint32_t)rx_data[4] << 24);
        int lost = (have_peer && peer_cnt != last_peer_cnt + 1) ? (int)(peer_cnt - last_peer_cnt - 1) : 0;
        have_peer = 1;
        last_peer_cnt = peer_cnt;
        int n = snprintf(msg, sizeof msg, "RX id=0x%03lX from node %u counter=%lu%s\r\n", (unsigned long)rx_hdr.StdId,
                         rx_data[0], (unsigned long)peer_cnt, lost > 0 ? "  <-- lost frames!" : "");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }

    if (err_flag)
    {
        err_flag = 0;
        int n = snprintf(msg, sizeof msg, "CAN error callback: HAL error code 0x%08lX\r\n", (unsigned long)err_code);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    }

    if (HAL_GetTick() - t_st >= 1000)
    {
        t_st += 1000;
        uint32_t esr = CAN1->ESR;                                      /* Error status register */
        uint32_t tec = (esr >> 16) & 0xFF, rec = (esr >> 24) & 0xFF;
        const char *state = (esr & CAN_ESR_BOFF) ? "BUS-OFF" : (esr & CAN_ESR_EPVF) ? "error passive" :
                            (esr & CAN_ESR_EWGF) ? "error warning" : "error active (ok)";
        int n = snprintf(msg, sizeof msg, "status: %s  TEC=%lu REC=%lu  sent=%lu tx-mailbox-aborts=%lu\r\n",
                         state, (unsigned long)tec, (unsigned long)rec, (unsigned long)counter, (unsigned long)tx_skipped);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
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
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
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
/* HAL 콜백 (ISR 문맥)                                                          */
/* ------------------------------------------------------------------------- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h)
{
    if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rx_hdr, rx_data) == HAL_OK && rx_hdr.DLC >= 5) rx_flag = 1;
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *h)
{
    err_code = HAL_CAN_GetError(h);
    err_flag = 1;
    __HAL_CAN_CLEAR_FLAG(h, CAN_FLAG_ERRI);                                /* MSR.ERRI 플래그 지움 */
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

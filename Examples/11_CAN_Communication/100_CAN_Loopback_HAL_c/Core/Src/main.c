/**
 * 100_CAN_Loopback_HAL_c  —  [계층: HAL]  bxCAN 루프백 모드 (트랜시버/상대 노드 없이 송수신 시험)
 *
 * 동작
 *   - CAN1 을 500kbps 루프백 모드로 설정. 500ms 마다 표준 ID 0x123, 8바이트(증가하는 카운터) 프레임을 송신.
 *   - 루프백 모드에서는 송신 프레임이 내부에서 수신 경로로 되돌아오므로, FIFO0 수신 인터럽트가 걸린다.
 *   - 수신 콜백이 프레임을 읽어 오면 main 이 송신 내용과 비교해 UART(115200)로 출력.
 *
 * 실제 버스에 연결하려면
 *   1) CAN_MODE_LOOPBACK -> CAN_MODE_NORMAL 로 변경 (아래 CAN_TEST_MODE)
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
#include "main.h"
#include <stdio.h>
#include <string.h>

#define CAN_TEST_MODE CAN_MODE_LOOPBACK      /* 실제 버스에서는 CAN_MODE_NORMAL */

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart2;

static volatile uint8_t rx_flag;
static CAN_RxHeaderTypeDef rx_hdr;
static uint8_t rx_data[8];

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_CAN_Init();

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

    char msg[120];
    uint8_t tx_data[8] = {0};
    uint32_t seq = 0;
    while (1)
    {
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
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }
        seq++;
        HAL_Delay(500);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): FIFO0 에 수신 프레임이 생겼을 때                          */
/* ------------------------------------------------------------------------- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_)
{
    if (HAL_CAN_GetRxMessage(hcan_, CAN_RX_FIFO0, &rx_hdr, rx_data) == HAL_OK)   /* RFxR.RFOM 으로 FIFO 해제 */
        rx_flag = 1;
}

void HAL_CAN_MspInit(CAN_HandleTypeDef *hc)
{
    if (hc->Instance != CAN1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_CAN1_CLK_ENABLE();                       /* RCC->APB1ENR.CAN1EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_11;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_PULLUP;           /* CAN_RX (CNF=10 풀업 입력) */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_12;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH; /* CAN_TX */
    HAL_GPIO_Init(GPIOA, &g);

    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;                           /* tq = 125ns */
    hcan.Init.Mode = CAN_TEST_MODE;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;             /* MCR.NART = 0: ACK 없으면 재전송 */
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK) Error_Handler();
}

/* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용 MSP: 클럭/핀만 설정 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();   /* RCC->APB1ENR.USART2EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN   */
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX: CNF=10, MODE=11 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX: CNF=01 (플로팅 입력) */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* HSI(8MHz)/2 x16 = 64MHz. Nucleo 는 HSE 가 ST-Link 에서 오므로 HSI 로 단순화 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    o.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();

    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2;          /* APB1 = 32MHz (최대 36MHz) */
    c.APB2CLKDivider = RCC_HCLK_DIV1;          /* APB2 = 64MHz */
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

/**
 * 03_CAN_Normal_2boards_HAL_c  —  [계층: HAL]  실제 CAN 버스(Normal 모드): 보드 2장 + 트랜시버, 오류 카운터와 버스오프 복구
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
#include "main.h"
#include <stdio.h>
#include <string.h>

#ifndef NODE_ID
#define NODE_ID 0
#endif

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart2;

static volatile uint8_t rx_flag, err_flag;
static volatile uint32_t err_code;
static CAN_RxHeaderTypeDef rx_hdr;
static uint8_t rx_data[8];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_CAN_Init();

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

    char msg[128];
    snprintf(msg, sizeof msg, "\r\n[CAN normal mode] node %d, tx id 0x%03X, 500kbps\r\n", NODE_ID, 0x100 + NODE_ID);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100);

    uint32_t counter = 0, t_tx = HAL_GetTick(), t_st = HAL_GetTick();
    uint32_t last_peer_cnt = 0;
    int have_peer = 0;
    uint32_t tx_skipped = 0;

    while (1)
    {
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
                             (uint8_t)(counter >> 24), (uint8_t)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) };
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
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
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
}

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

void HAL_CAN_MspInit(CAN_HandleTypeDef *hc)
{
    if (hc->Instance != CAN1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_11;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_PULLUP;              /* CAN_RX */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_12;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* CAN_TX */
    HAL_GPIO_Init(GPIOA, &g);
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 2, 0);          HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
}

static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;                                               /* 32MHz/4 = 8MHz tq, 16tq/비트 = 500kbps */
    hcan.Init.Mode = CAN_MODE_NORMAL;                                      /* 실제 버스 참여 (ACK 필요) */
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = ENABLE;                                         /* MCR.ABOM: 버스오프에서 하드웨어가 자동 복구 */
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;                                 /* ACK 를 못 받으면 재전송 */
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;   /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
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

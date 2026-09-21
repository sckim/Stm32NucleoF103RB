/**
 * 02_CAN_Filter_HAL_c  —  [계층: HAL]  bxCAN 수신 필터(Acceptance filter): ID 리스트 / 마스크 / 표준·확장 프레임, FIFO0·FIFO1 분배
 *
 * 01_CAN_Loopback 은 "모든 프레임 통과" 필터 하나만 썼다. 실제 CAN 버스에는 수많은 ID 가 흐르므로, 관심 있는 것만 하드웨어가 걸러
 * CPU 가 받는 프레임 수를 줄이는 것이 필터의 역할이다 (RM0008 24.7.4 Identifier filtering). 루프백 모드라 트랜시버 없이 시험 가능.
 *
 * 필터 뱅크 (F103xB: 14개 뱅크, 각 뱅크는 32비트 x2 레지스터)  — 이 예제는 3개 사용
 *   뱅크0  32비트 ID 리스트 : 표준 ID 0x100 과 0x200 "정확히" 일치하는 프레임만  -> FIFO0
 *   뱅크1  16비트 마스크    : 표준 ID 0x300~0x3FF (상위 3비트 0b011 일치, 마스크 0x700) 데이터 프레임 -> FIFO1
 *   뱅크2  32비트 마스크    : 확장(29비트) ID 의 ID[28:16] 이 0x18FF 인 것 (0x18FF0000~0x18FFFFFF) -> FIFO0
 *   그 외 ID(표준 0x400, 확장 0x1ABCDEF0)는 어떤 필터에도 안 맞아 수신되지 않는다.
 *
 * 동작: 다음 7개 프레임을 루프백으로 송신하고, 어떤 프레임이 어느 FIFO 로 수신됐는지 표로 출력 (UART 115200)
 *   STD 0x100 / STD 0x200 / STD 0x300 / STD 0x3A5 / STD 0x400 / EXT 0x18FF0001 / EXT 0x1ABCDEF0
 *   기대: 0x100,0x200,EXT 0x18FF0001 -> FIFO0 / 0x300,0x3A5 -> FIFO1 / 0x400, EXT 0x1ABCDEF0 -> 수신 안 됨
 *
 * 필터 레지스터 인코딩 (32비트 스케일): ID 레지스터 = STDID<<21 | EXTID<<3 | IDE<<2 | RTR<<1
 *   표준 ID X : High = X<<5, Low = 0        확장 ID E : High = E>>13, Low = ((E<<3) & 0xFFFF) | IDE(0x4)
 *   16비트 스케일: 각 16비트 필터 = STDID<<5 | RTR<<4 | IDE<<3 | EXTID[17:15]. 뱅크 1개가 (ID, 마스크) 쌍 2개를 담는다.
 *   마스크 모드: 마스크의 1 인 비트는 ID 와 "반드시 일치", 0 인 비트는 "무시".   리스트 모드: 두 ID 와 정확히 일치.
 *
 * ISR vs 콜백
 *   USB_LP_CAN1_RX0_IRQHandler / CAN1_RX1_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_CAN_IRQHandler()
 *     -> HAL_CAN_RxFifo0MsgPendingCallback() / HAL_CAN_RxFifo1MsgPendingCallback() (이 파일)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart2;

typedef struct { uint32_t id; uint8_t ext; uint8_t fifo; } rx_rec_t;
static rx_rec_t rxlog[16];
static volatile uint8_t rx_n;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);
static void filters_init(void);

static const struct { uint32_t id; uint8_t ext; } tx_list[7] = {
    { 0x100, 0 }, { 0x200, 0 }, { 0x300, 0 }, { 0x3A5, 0 }, { 0x400, 0 }, { 0x18FF0001UL, 1 }, { 0x1ABCDEF0UL, 1 },
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_CAN_Init();
    filters_init();

    if (HAL_CAN_Start(&hcan) != HAL_OK) Error_Handler();
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING);

    char msg[96];
    while (1)
    {
        rx_n = 0;
        for (int i = 0; i < 7; i++)
        {
            CAN_TxHeaderTypeDef h = {0};
            if (tx_list[i].ext) { h.ExtId = tx_list[i].id; h.IDE = CAN_ID_EXT; }
            else                { h.StdId = tx_list[i].id; h.IDE = CAN_ID_STD; }
            h.RTR = CAN_RTR_DATA;
            h.DLC = 2;
            h.TransmitGlobalTime = DISABLE;
            uint8_t d[2] = { (uint8_t)i, 0xA5 };
            uint32_t mb;
            uint32_t t0 = HAL_GetTick();
            while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0 && (HAL_GetTick() - t0) < 20) {}   /* 빈 메일박스 대기 */
            HAL_CAN_AddTxMessage(&hcan, &h, d, &mb);
        }
        HAL_Delay(50);                                                   /* 루프백 수신이 끝나길 기다림 */

        HAL_UART_Transmit(&huart2, (uint8_t *)"--- round ---\r\n", 15, 100);
        for (int i = 0; i < 7; i++)
        {
            int fifo = -1;
            for (int k = 0; k < rx_n; k++)
                if (rxlog[k].id == tx_list[i].id && rxlog[k].ext == tx_list[i].ext) fifo = rxlog[k].fifo;
            int n = snprintf(msg, sizeof msg, "%s 0x%08lX  ->  %s\r\n", tx_list[i].ext ? "EXT" : "STD", (unsigned long)tx_list[i].id,
                             fifo < 0 ? "filtered out (not received)" : (fifo == 0 ? "FIFO0" : "FIFO1"));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
        HAL_Delay(2000);
    }
}

static void filters_init(void)
{
    CAN_FilterTypeDef f = {0};
    f.SlaveStartFilterBank = 14;                                         /* CAN1 이 뱅크 0..13 을 전부 사용 */

    /* 뱅크0: 32비트 ID 리스트 (0x100, 0x200) -> FIFO0 */
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDLIST;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh     = (uint16_t)(0x100u << 5);   f.FilterIdLow     = 0;    /* 첫 번째 ID = 표준 0x100 */
    f.FilterMaskIdHigh = (uint16_t)(0x200u << 5);   f.FilterMaskIdLow = 0;    /* 리스트 모드에서는 이 필드가 "두 번째 ID" */
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation = ENABLE;
    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) Error_Handler();

    /* 뱅크1: 16비트 마스크. 표준 0x3xx 데이터 프레임 -> FIFO1.  (16비트: STDID<<5 | RTR<<4 | IDE<<3) */
    f.FilterBank = 1;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_16BIT;
    f.FilterIdLow      = (uint16_t)(0x300u << 5);                         /* 필터 A ID  : 표준 0x300, RTR=0, IDE=0 */
    f.FilterMaskIdLow  = (uint16_t)((0x700u << 5) | (1u << 4) | (1u << 3)); /* 필터 A 마스크: STDID 상위 3비트 + RTR + IDE 일치 */
    f.FilterIdHigh     = f.FilterIdLow;                                   /* 필터 B 는 A 와 동일하게(중복) */
    f.FilterMaskIdHigh = f.FilterMaskIdLow;
    f.FilterFIFOAssignment = CAN_RX_FIFO1;
    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) Error_Handler();

    /* 뱅크2: 32비트 마스크. 확장 ID[28:16] = 0x18FF 인 프레임 -> FIFO0 */
    uint32_t id  = (0x18FF0000UL << 3) | 0x4u;                            /* EXTID<<3 | IDE */
    uint32_t msk = (0x1FFF0000UL << 3) | 0x4u;                            /* ID[28:16](13비트)과 IDE 비트가 일치해야 통과 */
    f.FilterBank = 2;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh     = (uint16_t)(id >> 16);   f.FilterIdLow     = (uint16_t)(id & 0xFFFF);
    f.FilterMaskIdHigh = (uint16_t)(msk >> 16);  f.FilterMaskIdLow  = (uint16_t)(msk & 0xFFFF);
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) Error_Handler();
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 필터를 통과한 프레임이 FIFO 에 도착                        */
/* ------------------------------------------------------------------------- */
static void record(uint32_t fifo)
{
    CAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    if (HAL_CAN_GetRxMessage(&hcan, fifo, &rh, d) == HAL_OK && rx_n < 16)
    {
        rxlog[rx_n].ext = (rh.IDE == CAN_ID_EXT);
        rxlog[rx_n].id = rxlog[rx_n].ext ? rh.ExtId : rh.StdId;
        rxlog[rx_n].fifo = (fifo == CAN_RX_FIFO0) ? 0 : 1;
        rx_n++;
    }
}
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h) { (void)h; record(CAN_RX_FIFO0); }
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *h) { (void)h; record(CAN_RX_FIFO1); }

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
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 1, 0);          HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
}

static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;                     /* 32MHz/4 = 8MHz tq, 16tq/비트 = 500kbps (01_CAN_Loopback 참고) */
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

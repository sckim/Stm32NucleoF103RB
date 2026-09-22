/**
 * 04_SPI_Slave_Loopback_HAL_c  —  [계층: HAL]  SPI 슬레이브 모드: 보드 한 장 안에서 SPI1(마스터) <-> SPI2(슬레이브) 시험
 *
 * 배선 (점퍼선 4개)
 *   PA5 (SPI1_SCK)  ---- PB13 (SPI2_SCK)
 *   PA7 (SPI1_MOSI) ---- PB15 (SPI2_MOSI)
 *   PB14 (SPI2_MISO) ---- PA6 (SPI1_MISO)
 *   PA4 (마스터가 GPIO 로 제어하는 CS) ---- PB12 (SPI2_NSS, 슬레이브 하드웨어 NSS 입력)
 *   (PA5 는 온보드 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다)
 *
 * SPI 마스터/슬레이브의 차이
 *   마스터가 SCK 를 만들고 NSS(CS)로 슬레이브를 선택한다. 슬레이브는 SCK 를 받아 데이터를 시프트하며, 마스터가 클럭을 주는 순간
 *   슬레이브의 "송신 데이터가 이미 준비되어 있어야" 한다 (전이중이라 수신과 송신이 같은 클럭에 일어남).
 *   -> 슬레이브는 응답을 미리 장전(arm)해 두고, 한 프레임이 끝나면 다음 프레임 응답을 다시 장전한다.
 *   그래서 슬레이브의 응답은 "직전 프레임에 대한 결과"가 되어 한 프레임 늦게 도착한다 (SPI 센서/메모리가 흔히 쓰는 파이프라인 방식).
 *
 * 동작 프로토콜 (8바이트 프레임)
 *   마스터가 [n, n+1, ..., n+7] 을 보내면, 슬레이브는 그것을 받으면서 동시에 "직전 프레임의 각 바이트를 비트 반전한 값"을 돌려준다.
 *   마스터는 받은 값이 (직전에 보낸 값 ^ 0xFF) 인지 검사해 PASS/FAIL 을 UART(115200)로 출력한다.
 *
 * 하드웨어 대응
 *   SPI2->CR1: MSTR=0(슬레이브), SSM=0(하드웨어 NSS), CPOL=0/CPHA=0(마스터와 동일해야 함).  SPI2 는 APB1(32MHz)에 연결.
 *   마스터 SPI1: 64MHz/64 = 1MHz.   슬레이브 SCK 최대는 f_PCLK/... 이내이면 되므로 1MHz 는 충분.
 *
 * ISR vs 콜백
 *   SPI2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_SPI_IRQHandler()
 *     -> HAL_SPI_TxRxCpltCallback() (이 파일) : 슬레이브가 한 프레임을 다 주고받았을 때 (마스터 쪽은 폴링이라 콜백 없음)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define FRAME 8

SPI_HandleTypeDef hspi1;      /* 마스터 */
SPI_HandleTypeDef hspi2;      /* 슬레이브 */
UART_HandleTypeDef huart2;

static uint8_t s_tx[FRAME];   /* 슬레이브가 다음 프레임에 내보낼 데이터 */
static uint8_t s_rx[FRAME];   /* 슬레이브가 받은 데이터 */
static volatile uint32_t slave_frames;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Master_Init(void);
static void MX_SPI2_Slave_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Master_Init();
    MX_SPI2_Slave_Init();

    memset(s_tx, 0x00, sizeof s_tx);
    if (HAL_SPI_TransmitReceive_IT(&hspi2, s_tx, s_rx, FRAME) != HAL_OK) Error_Handler();   /* 슬레이브: 첫 프레임 장전 */

    char msg[128];
    uint8_t sent[FRAME] = {0}, prev_sent[FRAME] = {0}, got[FRAME];
    uint32_t pass = 0, fail = 0;
    for (uint32_t n = 0; ; n++)
    {
        memcpy(prev_sent, sent, sizeof sent);
        for (int i = 0; i < FRAME; i++) sent[i] = (uint8_t)(n * 8 + i);

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);                 /* CS Low: 슬레이브 선택 */
        HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, sent, got, FRAME, 100);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);                   /* CS High: 프레임 종료 */

        HAL_Delay(20);                                                        /* 슬레이브가 다음 프레임을 장전할 시간 */

        int ok = 1;
        if (n > 0)                                                            /* 첫 프레임의 응답은 초기값이라 검사 제외 */
            for (int i = 0; i < FRAME; i++) if (got[i] != (uint8_t)(prev_sent[i] ^ 0xFF)) ok = 0;
        if (st != HAL_OK) ok = 0;
        if (n > 0) { if (ok) pass++; else fail++; }

        int len = snprintf(msg, sizeof msg, "#%lu  sent %02X..%02X  got %02X %02X %02X %02X ...  slave frames=%lu  %s (pass=%lu fail=%lu)\r\n",
                           (unsigned long)n, sent[0], sent[FRAME - 1], got[0], got[1], got[2], got[3],
                           (unsigned long)slave_frames, n == 0 ? "-" : (ok ? "PASS" : "FAIL"),
                           (unsigned long)pass, (unsigned long)fail);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)len, 100);
        HAL_Delay(480);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 슬레이브가 한 프레임 송수신 완료 -> 다음 응답을 만들어 다시 장전       */
/* ------------------------------------------------------------------------- */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) return;
    for (int i = 0; i < FRAME; i++) s_tx[i] = (uint8_t)(s_rx[i] ^ 0xFF);      /* 다음 프레임에서 돌려줄 응답 = 이번에 받은 값의 반전 */
    slave_frames++;
    HAL_SPI_TransmitReceive_IT(hspi, s_tx, s_rx, FRAME);                      /* 다음 프레임 장전 */
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)                          /* 오버런/모드 오류 시 슬레이브 재장전 */
{
    if (hspi->Instance == SPI2) HAL_SPI_TransmitReceive_IT(hspi, s_tx, s_rx, FRAME);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef g = {0};
    if (hspi->Instance == SPI1)                                /* 마스터: SCK/MOSI = AF_PP, MISO = 입력 */
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_5 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_6;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else if (hspi->Instance == SPI2)                           /* 슬레이브: SCK/MOSI/NSS = 입력, MISO = AF_PP */
    {
        __HAL_RCC_SPI2_CLK_ENABLE();                           /* RCC->APB1ENR.SPI2EN */
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;   /* NSS, SCK, MOSI */
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_14;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;                    /* MISO */
        HAL_GPIO_Init(GPIOB, &g);
        HAL_NVIC_SetPriority(SPI2_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(SPI2_IRQn);
    }
}

static void MX_SPI1_Master_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;                             /* CS 는 PA4 GPIO 로 직접 제어 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;   /* 64MHz / 64 = 1MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_SPI2_Slave_Init(void)
{
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_SLAVE;                          /* CR1.MSTR = 0 */
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;                 /* 마스터와 동일한 모드 0 */
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_HARD_INPUT;                       /* NSS 핀이 Low 일 때만 슬레이브 동작 */
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;    /* 슬레이브에서는 무시됨 */
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);        /* CS 비활성 */
    g.Pin = GPIO_PIN_4;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_HIGH;
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

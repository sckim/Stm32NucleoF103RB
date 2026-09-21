/**
 * 40_SPI_Loopback_HAL_c  —  [계층: HAL]  SPI1 마스터 루프백 + 클럭 분주별 전송 속도 측정
 *
 * 배선: PA7 (MOSI, 'D11') ---- 점퍼선 ---- PA6 (MISO, 'D12')     (슬레이브 장치 없이 마스터만으로 시험)
 *       SCK = PA5 ('D13') 은 온보드 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다(정상).
 *
 * 동작
 *   - 256바이트 패턴을 SPI 로 송신하면서 동시에 수신(전이중)하고, 송신 == 수신인지 비교해 PASS/FAIL 출력.
 *   - 분주비 /256, /64, /16, /4 로 바꿔 가며 전송 시간을 DWT 사이클로 재고 실제 비트율을 출력.
 *   - PA7-PA6 점퍼를 빼면 수신이 전부 0x00/0xFF 가 되어 FAIL 이 나온다 (음성 대조).
 *
 * SPI 설정 (RM0008 25장)
 *   SPI1 클럭 = APB2 = 64MHz.  SPI1->CR1.BR[2:0] : /2 ~ /256.   /64 -> 1MHz, /4 -> 16MHz
 *   CR1.MSTR=1(마스터), CR1.CPOL=0 / CPHA=0 (모드 0: 유휴 Low, 첫 에지에서 샘플), 8비트, MSB 먼저
 *   CR1.SSM=1, SSI=1 (NSS 를 소프트웨어로 관리)
 *   전이중 1바이트: DR 에 쓰면 송신 시프트 시작 -> 같은 클럭에 수신 -> SR.RXNE 후 DR 읽기
 *
 * 인터럽트/DMA 를 사용하지 않는다 (HAL_SPI_TransmitReceive 폴링). DMA 버전은 41_SPI_DMA_Fullduplex 참고.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define LEN 256

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

static uint8_t tx[LEN], rx[LEN];

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(uint32_t prescaler);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;         /* DWT 사이클 카운터 켜기 */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (int i = 0; i < LEN; i++) tx[i] = (uint8_t)(i * 7 + 3);

    static const struct { uint32_t reg; uint32_t div; } cfg[] = {
        { SPI_BAUDRATEPRESCALER_256, 256 }, { SPI_BAUDRATEPRESCALER_64, 64 },
        { SPI_BAUDRATEPRESCALER_16, 16 },   { SPI_BAUDRATEPRESCALER_4, 4 },
    };

    char msg[112];
    while (1)
    {
        for (unsigned k = 0; k < sizeof cfg / sizeof cfg[0]; k++)
        {
            MX_SPI1_Init(cfg[k].reg);
            memset(rx, 0, sizeof rx);

            uint32_t t0 = DWT->CYCCNT;
            HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, LEN, 100);
            uint32_t cyc = DWT->CYCCNT - t0;

            int ok = (st == HAL_OK) && (memcmp(tx, rx, LEN) == 0);
            uint32_t bits = LEN * 8U;
            uint32_t kbps = (uint32_t)(((uint64_t)bits * 64000ULL) / cyc);   /* bit / (cyc/64MHz) 를 kbit/s 로 */
            int n = snprintf(msg, sizeof msg, "/%-3lu SCK=%5lu kHz(설정)  측정=%5lu kbit/s (오버헤드 포함)  %s\r\n",
                             (unsigned long)cfg[k].div, (unsigned long)(64000UL / cfg[k].div),
                             (unsigned long)kbps, ok ? "PASS" : "FAIL");
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
        }
        HAL_UART_Transmit(&huart2, (uint8_t *)"----\r\n", 6, 100);
        HAL_Delay(2000);
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_SPI1_CLK_ENABLE();            /* RCC->APB2ENR.SPI1EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;        /* SCK, MOSI: 대체기능 푸시풀 50MHz */
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_6;                     /* MISO: 마스터에서는 입력(플로팅) */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_SPI1_Init(uint32_t prescaler)
{
    if (hspi1.State != HAL_SPI_STATE_RESET) HAL_SPI_DeInit(&hspi1);   /* 분주비 변경 시 재초기화 */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;          /* CPOL = 0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;              /* CPHA = 0 -> SPI 모드 0 */
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = prescaler;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
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

/**
 * 08_ADC_SampleTime_SourceZ_Reg_c  —  [계층: 레지스터(CMSIS)]  샘플링 시간과 소스 임피던스: 샘플-홀드 커패시터가 덜 충전될 때의 오차
 *
 * 교안 「ADC」(주변장치 3) 6.2절(샘플-홀드와 입력 임피던스)의 실습이다.
 *
 * 원리
 *   ADC는 샘플링 시간 Ts 동안 내부 커패시터 C_ADC(최대 8 pF)를 입력 전압까지 충전한 뒤 끊고 12.5 사이클 동안 축차 비교한다.
 *   충전 경로는 외부 소스 저항 R_AIN과 샘플링 스위치 R_ADC(최대 1 kΩ)의 직렬 RC이다.
 *   오차를 1/4 LSB 이하로 하려면 Ts ≥ (R_AIN + R_ADC)·C_ADC·ln(2^(N+2)), 즉
 *     R_AIN < Ts / (f_ADC · C_ADC · ln(2^14)) − R_ADC          (데이터시트 DS5319 식 1, 표 48)
 *   C_ADC에는 직전 채널의 전압이 남아 있으므로, 직전에 0 V 채널을 변환하면 덜 충전된 만큼 값이 낮게 나온다.
 *
 * 실험
 *   정규 시퀀스 2개: 랭크 1 = CH1(PA1, GND에 연결, 239.5 사이클로 C_ADC를 0 V로 비움), 랭크 2 = CH0(PA0, 시험 입력).
 *   CH0의 샘플링 시간 SMP0을 000(1.5)~111(239.5)로 바꿔 가며 64회 평균하고, 239.5 사이클 값을 기준으로 오차(LSB)를 낸다.
 *   스캔 모드에서 EOC는 시퀀스 마지막(CH0) 변환 뒤에만 서고 그때 DR에는 CH0 결과가 있으므로 DMA 없이 읽을 수 있다.
 *
 * 배선 (Nucleo-F103RB)
 *   PA1(A1) ── GND
 *   3V3 ── 10 kΩ ──┬── 10 kΩ ── GND        (약 1.65 V, 테브난 저항 5 kΩ)
 *                  └── R_S ── PA0(A0)       R_S = 0(점퍼) / 10 kΩ / 47 kΩ / 100 kΩ 로 바꿔 가며 비교
 *   PA0 근처에 별도 커패시터는 달지 않는다(달면 외부 C가 전하를 공급해 오차가 줄어든다 — 그것도 실험해 볼 것).
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB2 ÷1 = 64 MHz, ADCPRE ÷6 → ADCCLK 10.67 MHz (최대 14 MHz)
 *
 * 사용 레지스터·비트 (RM0008 11.12절)
 *   RCC_CFGR.ADCPRE[15:14] = 10 (÷6), RCC_APB2ENR.ADC1EN
 *   ADC1->CR2 : ADON(bit0) 켜기·변환 시작, CAL(bit2), RSTCAL(bit3), EXTSEL[19:17] = 111(SWSTART), EXTTRIG(bit20), SWSTART(bit22)
 *   ADC1->CR1 : SCAN(bit8)
 *   ADC1->SMPR2 : SMP0[2:0], SMP1[5:3]
 *   ADC1->SQR1.L[23:20] = 1 (2개 변환), ADC1->SQR3 : SQ1[4:0] = 1, SQ2[9:5] = 0
 *   ADC1->SR.EOC(bit1) : DR을 읽으면 지워진다
 *
 * 인터럽트를 사용하지 않는다(폴링). 출력: USART2(PA2) 115200 8N1.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define N_AVG       64u

/* SMP 코드별 샘플링 사이클(×10)과, ADCCLK 10.67 MHz에서 식 1로 계산한 R_AIN 최대값(×0.1 kΩ).
 * 데이터시트 표 47의 상한(50 kΩ)을 넘는 값은 500으로 자른다. python 검산값: 0.8 / 8.1 / 15.3 / 33.4 / 49.1 / 66.0 / 85.3 / 288 kΩ */
static const uint16_t kSmpX10[8]   = { 15, 75, 135, 285, 415, 555, 715, 2395 };
static const uint16_t kRainMax[8]  = { 8, 81, 153, 334, 491, 500, 500, 500 };

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 WS (48~72 MHz) */
    /* HSI/2 × 16, AHB ÷1, APB1 ÷2, APB2 ÷1, ADCPRE ÷6 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClockUpdate();
}

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < us * CYC_PER_US) {}
}

static void UART2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 50 MHz */
    USART2->BRR = (PCLK1_HZ + 115200u / 2u) / 115200u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_putu_w(uint32_t v, int w)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i < w) { b[i++] = ' '; }
    while (i) uart_putc(b[--i]);
}
static void uart_puti_w(int32_t v, int w)       /* 부호 있는 정수, 오른쪽 정렬 */
{
    char b[12]; int i = 0; uint32_t u = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
    do { b[i++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) b[i++] = '-';
    while (i < w) { b[i++] = ' '; }
    while (i) uart_putc(b[--i]);
}
static void uart_put_x10(uint32_t v10, int w)   /* 값×10 → "정수.소수1자리" */
{
    uart_putu_w(v10 / 10u, w - 2); uart_putc('.'); uart_putc((char)('0' + v10 % 10u));
}

/* ---------------- ADC ---------------- */

static void ADC1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_IOPAEN;
    /* PA0, PA1 = 아날로그 입력: CRL의 CNF=00, MODE=00 (디지털 입력 버퍼 차단) */
    GPIOA->CRL &= ~((0xFu << 0) | (0xFu << 4));

    ADC1->CR1 = ADC_CR1_SCAN;                           /* 스캔 모드: SQR에 적은 순서대로 변환 */
    ADC1->CR2 = ADC_CR2_ADON;                           /* 첫 ADON = 1: 전원 켜기(변환은 시작하지 않음) */
    delay_us(10);                                       /* tSTAB ≤ 1 µs (DS 표 47), 여유 있게 */

    /* 보정: ADON = 1 상태로 ADC 클럭 2주기 이상 지난 뒤 RSTCAL → CAL (RM0008 11.4) */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL) {}

    /* 시퀀스: 랭크1 = CH1(GND), 랭크2 = CH0(시험 입력). L = 1 → 변환 2개 */
    ADC1->SQR1 = (1u << 20);
    ADC1->SQR3 = (1u << 0) | (0u << 5);

    /* 변환 시작원: SWSTART (EXTSEL = 111, EXTTRIG = 1) */
    ADC1->CR2 |= ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;
}

/* CH0의 샘플링 시간 코드를 바꾸고 N_AVG회 평균한 CH0 결과를 돌려준다. CH1은 항상 239.5 사이클. */
static uint32_t Measure(uint32_t smp0)
{
    ADC1->SMPR2 = (smp0 << 0) | (7u << 3);              /* SMP0 = smp0, SMP1 = 111(239.5) */
    uint32_t sum = 0;
    for (uint32_t k = 0; k < N_AVG; k++)
    {
        ADC1->CR2 |= ADC_CR2_SWSTART;                   /* 시퀀스 1회 시작 */
        while (!(ADC1->SR & ADC_SR_EOC)) {}             /* 스캔 모드: 마지막 랭크(CH0) 뒤에만 EOC */
        sum += ADC1->DR & 0x0FFFu;                      /* DR 읽기 → EOC 해제 */
    }
    return (sum + N_AVG / 2u) / N_AVG;
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */
    ADC1_Init();

    uart_puts("\r\n08_ADC_SampleTime_SourceZ_Reg_c  ADCCLK 10.67 MHz, CH1(GND) -> CH0(test), 64-avg\r\n");
    uart_puts("wire: PA1-GND, PA0 <- R_S <- 1.65 V divider. Change R_S and compare.\r\n");

    while (1)
    {
        uint32_t raw[8];
        for (uint32_t s = 0; s < 8u; s++) raw[s] = Measure(s);
        int32_t ref = (int32_t)raw[7];

        uart_puts("SMP  Ts[cyc]  Ts[us]  RAINmax[kOhm]  CH0 raw   err[LSB]\r\n");
        for (uint32_t s = 0; s < 8u; s++)
        {
            /* 1 ADC 사이클 = 1/10.67 MHz = 0.09375 µs → Ts[µs]×100 = (사이클×10) × 0.9375 = (사이클×10) × 75 / 80 */
            uint32_t ts_x100 = (kSmpX10[s] * 75u + 40u) / 80u;
            uart_putu_w(s, 3);
            uart_put_x10(kSmpX10[s], 9);
            uart_putu_w(ts_x100 / 100u, 6); uart_putc('.');
            uart_putc((char)('0' + (ts_x100 / 10u) % 10u)); uart_putc((char)('0' + ts_x100 % 10u));
            uart_put_x10(kRainMax[s], 13);
            uart_puts(s >= 5u ? "*" : " ");
            uart_putu_w(raw[s], 9);
            uart_puti_w((int32_t)raw[s] - ref, 10);
            uart_puts("\r\n");
        }
        uart_puts("(* = DS table 47 limit 50 kOhm. err = vs 239.5-cycle value)\r\n\r\n");
        GPIOA->ODR ^= (1u << 5);
        delay_us(2000000u);
    }
}

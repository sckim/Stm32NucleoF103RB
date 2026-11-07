/**
 * 09_ADC_TriggerJitter_Reg_c  —  [계층: 레지스터(CMSIS)]  샘플링 간격 지터: 소프트웨어 시작 vs 타이머(TRGO) 하드웨어 트리거
 *
 * 교안 「ADC」(주변장치 3) 6.4절(변환 시작원과 트리거)과 6.7절(신호처리 관점)의 실습이다.
 *
 * 무엇을 재나
 *   1 kHz(1 ms 간격)로 ADC1 CH0을 1000번 변환하면서, 변환이 끝날 때마다 EOC 인터럽트에서 DWT->CYCCNT(64 MHz)를 기록한다.
 *   기록 간격의 평균·최소·최대·표준편차(σ)를 세 가지 시작 방식으로 비교한다.
 *     A) 소프트웨어, 지연 루프   : SWSTART → 가변 길이 작업(0~199 µs, 의사난수) → 1000 µs 지연 → 반복
 *                                  (HAL_Delay로 주기를 맞추는 흔한 코드의 모형: 평균 주기가 늘고 지터가 크다)
 *     B) 소프트웨어, 마감 시각   : 다음 마감 시각(+1000 µs)까지 DWT로 기다렸다가 SWSTART
 *                                  (평균은 맞지만 기다리는 동안 끼어드는 인터럽트만큼 흔들린다)
 *     C) 하드웨어, TIM3 TRGO     : TIM3 갱신 이벤트(1 kHz)가 ADC를 직접 시작(EXTSEL = 100)
 *   세 경우 모두 TIM2가 약 7.3 kHz로 5 µs짜리 "방해 인터럽트"를 낸다(우선순위 1, 실제 응용의 다른 ISR 모형).
 *   EOC 인터럽트는 우선순위 0이라 방해 ISR을 선점하므로, 기록 시점 자체의 흔들림은 몇 사이클 수준이다.
 *
 * 왜 중요한가 (교안 6.7절)
 *   샘플링 시각 지터 σ_t는 입력 주파수 f의 사인파에 대해 SNR ≤ −20·log10(2π·f·σ_t)의 상한을 만든다.
 *   생체신호 필터·FFT는 "같은 간격"을 전제로 하므로 간격이 흔들리면 스펙트럼에 잡음 바닥이 생긴다.
 *
 * 클럭: SYSCLK 64 MHz, APB1 ÷2(TIM2·TIM3 클럭 64 MHz), APB2 ÷1, ADCPRE ÷6 → ADCCLK 10.67 MHz
 *
 * 사용 레지스터·비트 (RM0008 11.12절, 15.4절)
 *   ADC1->CR2 : ADON, CAL, RSTCAL, EXTSEL[19:17] (111 = SWSTART, 100 = TIM3_TRGO), EXTTRIG(bit20), SWSTART(bit22)
 *   ADC1->CR1 : EOCIE(bit5)        ADC1->SMPR2.SMP0 = 101(55.5 사이클)       ADC1->SQR3.SQ1 = 0
 *   TIM3->CR2.MMS[6:4] = 010 (갱신 → TRGO), TIM3->PSC = 63, ARR = 999
 *   TIM2: PSC = 0, ARR = 8766 (≈ 7.30 kHz), DIER.UIE — 방해 인터럽트
 *   주의(ES096 2.6.2): 슬레이브(ADC) 클럭을 마스터 타이머보다 먼저 켜 두어야 트리거를 놓치지 않는다.
 *
 * ISR
 *   ADC1_2_IRQHandler : 진짜 ISR. DR을 읽어 EOC를 지우고 시각만 기록한다(우선순위 0).
 *   TIM2_IRQHandler   : 진짜 ISR. UIF를 지우고 5 µs 바쁜 대기(우선순위 1).
 *   HAL 콜백은 쓰지 않는다. 출력: USART2(PA2) 115200 8N1. 배선 없음(PA0은 열어 둬도 된다).
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define N_SAMP      1000u

static volatile uint32_t g_t[N_SAMP];           /* EOC 시각(DWT 사이클) */
static volatile uint32_t g_n;                   /* 기록된 개수 */
static volatile uint16_t g_last;                /* 마지막 변환값(관찰용) */

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 WS */
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
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX */
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
/* 사이클 수를 µs 소수 3자리로 출력 (64 사이클 = 1 µs) */
static void uart_put_cyc_us(uint32_t cyc, int w)
{
    uint32_t ns = (cyc * 1000u + CYC_PER_US / 2u) / CYC_PER_US;
    uart_putu_w(ns / 1000u, w - 4); uart_putc('.');
    uart_putc((char)('0' + (ns / 100u) % 10u)); uart_putc((char)('0' + (ns / 10u) % 10u)); uart_putc((char)('0' + ns % 10u));
}

/* ---------------- ISR ---------------- */

void ADC1_2_IRQHandler(void)
{
    uint32_t t = DWT->CYCCNT;                   /* 진입 즉시 시각 기록 */
    if (ADC1->SR & ADC_SR_EOC)
    {
        g_last = (uint16_t)ADC1->DR;            /* DR 읽기 → EOC 해제 */
        if (g_n < N_SAMP) g_t[g_n++] = t;
    }
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR = ~TIM_SR_UIF;
        uint32_t t0 = DWT->CYCCNT;
        while ((DWT->CYCCNT - t0) < 5u * CYC_PER_US) {}    /* 5 µs 일하는 척 */
    }
}

/* ---------------- 설정 ---------------- */

static void ADC1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(0xFu << 0);                 /* PA0 = 아날로그 입력 */
    ADC1->CR1 = ADC_CR1_EOCIE;                  /* 단일 채널, 스캔 없음, EOC 인터럽트 */
    ADC1->CR2 = ADC_CR2_ADON;                   /* 전원 켜기 */
    delay_us(10);
    ADC1->CR2 |= ADC_CR2_RSTCAL; while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
    ADC1->CR2 |= ADC_CR2_CAL;    while (ADC1->CR2 & ADC_CR2_CAL) {}
    ADC1->SMPR2 = (5u << 0);                    /* SMP0 = 55.5 사이클 → 변환 68 사이클 = 6.4 µs */
    ADC1->SQR1 = 0;                             /* L = 0 → 변환 1개 */
    ADC1->SQR3 = 0;                             /* SQ1 = CH0 */
}

static void SetTrigger(uint32_t extsel)
{
    /* EXTSEL 변경은 변환이 없을 때 한다. EXTTRIG = 1이어야 SWSTART든 TRGO든 받는다.
     * 함정: ADON = 1인 CR2에 "다른 비트는 그대로, ADON = 1"을 다시 쓰면 그 쓰기 자체가 변환을 시작한다(RM0008 11.12.3 ADON 주석).
     * 값이 같으면 쓰지 않는다. */
    uint32_t v = (ADC1->CR2 & ~ADC_CR2_EXTSEL) | (extsel << 17) | ADC_CR2_EXTTRIG;
    if (v != ADC1->CR2) ADC1->CR2 = v;
}

static void Timers_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    /* TIM3: 1 kHz 갱신 → TRGO (MMS = 010). 아직 시작하지 않는다 */
    TIM3->PSC = 64 - 1;
    TIM3->ARR = 1000 - 1;
    TIM3->CR2 = (2u << 4);
    TIM3->EGR = TIM_EGR_UG;
    /* TIM2: 방해 인터럽트 ≈ 7.30 kHz (1 ms와 정수배가 아니게) */
    TIM2->PSC = 0;
    TIM2->ARR = 8767 - 1;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 1);
    NVIC_SetPriority(ADC1_2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(ADC1_2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

/* 16비트 LFSR(x^16 + x^14 + x^13 + x^11 + 1): 작업 길이를 흔드는 의사난수 */
static uint16_t lfsr = 0xACE1u;
static uint32_t rnd(void)
{
    uint16_t bit = (uint16_t)(((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u);
    lfsr = (uint16_t)((lfsr >> 1) | (bit << 15));
    return lfsr;
}

/* ---------------- 통계 출력 ---------------- */

static void Report(const char *name)
{
    uint32_t n = g_n - 1u;                      /* 간격 개수 */
    uint32_t mn = 0xFFFFFFFFu, mx = 0;
    uint64_t sum = 0;
    for (uint32_t i = 1; i < g_n; i++)
    {
        uint32_t d = g_t[i] - g_t[i - 1];
        if (d < mn) mn = d;
        if (d > mx) mx = d;
        sum += d;
    }
    uint32_t mean = (uint32_t)((sum + n / 2u) / n);
    uint64_t var = 0;                           /* 분산(사이클²) */
    for (uint32_t i = 1; i < g_n; i++)
    {
        int32_t e = (int32_t)(g_t[i] - g_t[i - 1]) - (int32_t)mean;
        var += (uint64_t)((int64_t)e * e);
    }
    var /= n;
    uint32_t sd = 0;                            /* 정수 제곱근(사이클) */
    while ((uint64_t)(sd + 1u) * (sd + 1u) <= var) sd++;

    uart_puts(name);
    uart_puts(" mean");  uart_put_cyc_us(mean, 10);
    uart_puts(" min");   uart_put_cyc_us(mn, 10);
    uart_puts(" max");   uart_put_cyc_us(mx, 10);
    uart_puts(" p-p");   uart_put_cyc_us(mx - mn, 9);
    uart_puts(" sigma"); uart_put_cyc_us(sd, 9);
    uart_puts(" us\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */
    ADC1_Init();                                /* 슬레이브(ADC) 클럭을 먼저 켠다 */
    Timers_Init();

    uart_puts("\r\n09_ADC_TriggerJitter_Reg_c  1 kHz target, 1000 samples, disturbance ISR 5 us @ 7.3 kHz\r\n");

    while (1)
    {
        /* A) 소프트웨어 시작 + 가변 작업 + 고정 지연 */
        SetTrigger(7u);
        g_n = 0;
        while (g_n < N_SAMP)
        {
            ADC1->CR2 |= ADC_CR2_SWSTART;
            delay_us(rnd() % 200u);             /* 처리 시간이 매번 다르다 */
            delay_us(1000u);                    /* "1 ms 쉬기" */
        }
        Report("A) SW + delay   ");

        /* B) 소프트웨어 시작 + 마감 시각 대기 */
        SetTrigger(7u);
        g_n = 0;
        uint32_t next = DWT->CYCCNT + 1000u * CYC_PER_US;
        while (g_n < N_SAMP)
        {
            while ((int32_t)(DWT->CYCCNT - next) < 0) {}   /* 마감 시각까지 바쁜 대기(방해 ISR이 끼어들 수 있음) */
            ADC1->CR2 |= ADC_CR2_SWSTART;
            next += 1000u * CYC_PER_US;
            delay_us(rnd() % 200u);             /* 작업 길이는 A와 같게 흔든다 */
        }
        Report("B) SW deadline  ");

        /* C) 하드웨어 트리거: TIM3 TRGO → ADC */
        SetTrigger(4u);
        g_n = 0;
        TIM3->CNT = 0;
        TIM3->CR1 = TIM_CR1_CEN;
        while (g_n < N_SAMP)
        {
            delay_us(rnd() % 200u);             /* CPU가 무엇을 하든 시작 시각과 무관 */
        }
        TIM3->CR1 = 0;
        Report("C) TIM3 TRGO    ");

        uart_puts("\r\n");
        GPIOA->ODR ^= (1u << 5);
        delay_us(2000000u);
    }
}

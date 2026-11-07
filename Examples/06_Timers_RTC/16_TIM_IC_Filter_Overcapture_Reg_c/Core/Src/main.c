/**
 * 16_TIM_IC_Filter_Overcapture_Reg_c  —  [계층: 레지스터(CMSIS)]  입력 캡처의 디지털 필터(ICxF)와 오버캡처(CCxOF)
 *
 * 교안 「타이머(TIM)」 6.3절(입력 캡처)과 8절(설계상의 한계)의 실습이다.
 *
 * 테스트 신호(자급): TIM3_CH1(PA6, D12)이 1 kHz PWM을 낸다. TIM3 클럭 64 MHz, PSC = 0, ARR = 63999.
 *   CCR1 = 펄스 폭(카운트, 1 카운트 = 15.625 ns): 32000(500 µs) / 32(500 ns) / 12(187.5 ns) / 4(62.5 ns)
 *   짧은 펄스는 "글리치" 역할을 한다.
 * 측정: TIM2_CH1(PA0, A0) 입력 캡처, 상승 에지. 배선: PA6 ── 점퍼 ── PA0 (06·07 예제와 같다)
 *
 * 실험 A — 디지털 필터 (CCMR1.IC1F[7:4], RM0008 15.4.7)
 *   필터는 f_SAMPLING으로 N번 연속 같은 레벨을 봐야 전이로 인정한다. CKD = 00이므로 f_DTS = f_CK_INT = 64 MHz.
 *     IC1F = 0000 : 필터 없음
 *     IC1F = 0011 : f_SAMPLING = f_CK_INT, N = 8   → 약 125 ns 이상 유지된 레벨만 인정
 *     IC1F = 0101 : f_SAMPLING = f_DTS/2,  N = 8   → 약 250 ns 이상
 *   펄스 폭 × 필터 조합마다 1초 동안 캡처 횟수(CC1IF 인터럽트 수)를 센다. 기대: 1000이면 통과, 0이면 걸러짐.
 *   경계 근처(필터 길이 ≈ 펄스 폭)는 재동기화 지연 때문에 한두 샘플 차이로 결과가 갈릴 수 있다.
 *   핀 출력단과 점퍼의 대역폭 때문에 62.5 ns 펄스가 필터 없이도 일부 사라질 수 있다 — 이것도 관찰 대상이다.
 *
 * 실험 B — 오버캡처 (SR.CC1OF)
 *   CCR1은 값 하나만 담는다. CC1IF가 1인 상태에서 다시 캡처되면 이전 값이 덮어써지고 CC1OF가 선다.
 *   인터럽트를 끄고 main 루프가 SR을 10 ms마다(느림) / 0.5 ms마다(빠름) 확인해, 1초 동안 읽은 캡처 수와 오버캡처 수를 센다.
 *   CC1IF는 CCR1을 읽으면 지워지고, CC1OF는 0을 써야 지워진다(rc_w0).
 *   기대: 10 ms 폴링 → 캡처 약 100, 오버캡처 약 100 / 0.5 ms 폴링 → 캡처 약 1000, 오버캡처 0.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2 → TIM2·TIM3 클럭 64 MHz
 *
 * 사용 레지스터·비트 (RM0008 15.4절)
 *   TIM2: CCMR1.CC1S[1:0] = 01(IC1 ← TI1), IC1F[7:4], IC1PSC[3:2] = 00 / CCER.CC1E, CC1P / DIER.CC1IE / SR.CC1IF, CC1OF / CCR1
 *         CC1S는 CC1E = 0일 때만 쓸 수 있으므로, 필터를 바꿀 때도 CC1E를 끄고 바꾼 뒤 다시 켠다.
 *   TIM3: CCMR1.OC1M = 110, OC1PE / CCER.CC1E / PSC, ARR, CCR1
 *
 * ISR: TIM2_IRQHandler(실험 A에서만 사용). CCR1을 읽어 CC1IF를 지우고 횟수만 센다. HAL 콜백은 쓰지 않는다.
 * 출력: USART2 115200 8N1.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)

static volatile uint32_t g_cap;                 /* 실험 A: 캡처 인터럽트 수 */
static volatile uint16_t g_last;                /* 마지막 캡처 값(관찰용) */

static const uint16_t kWidth[]  = { 32000, 32, 12, 4 };        /* TIM3 CCR1 (펄스 폭 카운트) */
static const char    *kWidthS[] = { "500 us  ", "500 ns  ", "187.5 ns", "62.5 ns " };
static const uint8_t  kFilt[]   = { 0x0, 0x3, 0x5 };           /* IC1F */

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;
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
static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) uart_putc(b[--i]);
}
static void uart_putu_w(uint32_t v, int w)      /* 오른쪽 정렬 */
{
    uint32_t t = v; int d = 1;
    while (t >= 10u) { t /= 10u; d++; }
    while (d++ < w) uart_putc(' ');
    uart_putu(v);
}

/* ---------------- ISR ---------------- */

/* 진짜 ISR. CCR1을 읽으면 CC1IF가 하드웨어로 지워진다(RM0008 15.4.5). */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_CC1IF)
    {
        g_last = (uint16_t)TIM2->CCR1;
        g_cap++;
    }
    if (TIM2->SR & TIM_SR_CC1OF) TIM2->SR = ~TIM_SR_CC1OF;
}

/* ---------------- 설정 ---------------- */

static void TestSignal_Init(void)
{
    /* PA6 = TIM3_CH1: AF 푸시풀 50 MHz (CRL[27:24] = 0xB) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 24)) | (0xBu << 24);
    TIM3->CR1   = TIM_CR1_ARPE;
    TIM3->PSC   = 0;                            /* 64 MHz, 1 카운트 = 15.625 ns */
    TIM3->ARR   = 64000 - 1;                    /* 1 kHz */
    TIM3->CCR1  = kWidth[0];
    TIM3->CCMR1 = (6u << 4) | TIM_CCMR1_OC1PE;  /* PWM 모드 1 */
    TIM3->CCER  = TIM_CCER_CC1E;
    TIM3->EGR   = TIM_EGR_UG;
    TIM3->CR1  |= TIM_CR1_CEN;
}

static void Capture_Init(void)
{
    /* PA0 = TIM2_CH1 입력: 리셋값(입력 플로팅, CRL[3:0] = 0x4) 그대로 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 0)) | (0x4u << 0);
    TIM2->CR1   = 0;                            /* CKD = 00 → f_DTS = f_CK_INT */
    TIM2->PSC   = 64 - 1;                       /* 1 MHz: 캡처 값은 µs 단위 */
    TIM2->ARR   = 0xFFFF;
    TIM2->CCMR1 = 1u;                           /* CC1S = 01, IC1F = 0000, IC1PSC = 00 */
    TIM2->CCER  = TIM_CCER_CC1E;                /* CC1P = 0: 상승 에지, 캡처 허용 */
    TIM2->EGR   = TIM_EGR_UG;
    TIM2->SR    = 0;
    TIM2->CR1  |= TIM_CR1_CEN;
}

static void SetFilter(uint8_t f)
{
    TIM2->CCER &= ~TIM_CCER_CC1E;               /* 채널 끄고 */
    TIM2->CCMR1 = 1u | ((uint32_t)f << 4);      /* CC1S = 01 유지, IC1F 변경 */
    TIM2->SR    = 0;
    TIM2->CCER |= TIM_CCER_CC1E;                /* 다시 켠다 */
}

/* ---------------- 실험 ---------------- */

static void Test_Filter(void)
{
    uart_puts("A) captures per second (input: 1 kHz pulses)\r\n");
    uart_puts("   pulse width | IC1F=0000 | IC1F=0011(~125ns) | IC1F=0101(~250ns)\r\n");
    TIM2->DIER = TIM_DIER_CC1IE;
    for (uint32_t w = 0; w < sizeof kWidth / sizeof kWidth[0]; w++)
    {
        TIM3->CCR1 = kWidth[w];                 /* 다음 주기부터 반영(OC1PE = 1) */
        delay_us(5000);
        uart_puts("   ");
        uart_puts(kWidthS[w]);
        uart_puts("    |");
        for (uint32_t k = 0; k < sizeof kFilt; k++)
        {
            SetFilter(kFilt[k]);
            g_cap = 0;
            delay_us(1000000u);
            uart_putu_w(g_cap, k == 0 ? 10 : 18);
            uart_puts(k == 2 ? "\r\n" : " |");
        }
    }
    TIM2->DIER = 0;
}

static void Test_Overcapture(uint32_t poll_us)
{
    uint32_t caps = 0, ovc = 0;
    TIM3->CCR1 = kWidth[0];
    SetFilter(0);
    delay_us(5000);
    TIM2->SR = 0;

    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < 1000000u * CYC_PER_US)
    {
        uint32_t sr = TIM2->SR;
        if (sr & TIM_SR_CC1IF)
        {
            g_last = (uint16_t)TIM2->CCR1;      /* 데이터를 먼저 읽는다 → CC1IF 해제 */
            caps++;
        }
        if (sr & TIM_SR_CC1OF)
        {
            ovc++;
            TIM2->SR = ~TIM_SR_CC1OF;           /* 0을 써서 해제 */
        }
        delay_us(poll_us);
    }
    uart_puts("   poll every ");
    uart_putu_w(poll_us, 5);
    uart_puts(" us : captures read = ");
    uart_putu_w(caps, 4);
    uart_puts(" , overcapture (CC1OF) = ");
    uart_putu_w(ovc, 4);
    uart_puts("\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */

    TestSignal_Init();
    Capture_Init();
    NVIC_SetPriority(TIM2_IRQn, 1);
    NVIC_EnableIRQ(TIM2_IRQn);

    uart_puts("\r\n16_TIM_IC_Filter_Overcapture_Reg_c  (wire PA6 -> PA0)\r\n");

    while (1)
    {
        Test_Filter();
        uart_puts("B) overcapture with polling (interrupt off, 500 us pulses at 1 kHz)\r\n");
        Test_Overcapture(10000u);
        Test_Overcapture(500u);
        uart_puts("\r\n");
        GPIOA->ODR ^= (1u << 5);
    }
}

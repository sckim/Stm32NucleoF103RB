/**
 * 15_TIM_CenterAligned_PWM_Reg_c  —  [계층: 레지스터(CMSIS)]  가장자리 정렬 PWM과 중앙 정렬 PWM 비교 + 타이머 동기 시작
 *
 * 교안 「타이머(TIM)」 6.2절(출력 비교와 PWM)과 6.4절(TRGO·ITR 연결)의 실습이다.
 *
 * 무엇을 하나
 *   같은 카운터 클럭(1 MHz), 같은 ARR(999), 같은 CCR로 두 타이머가 PWM을 낸다.
 *     TIM2_CH1(PA0, A0)  : 가장자리 정렬(CR1.CMS = 00), 업카운트  → 주기 = ARR+1 = 1000 µs (1 kHz)
 *     TIM3_CH1(PA6, D12) : 중앙 정렬 모드 1(CR1.CMS = 01)          → 주기 = 2×ARR = 1998 µs (약 500.5 Hz)
 *   두 출력 모두 PWM 모드 1(CCMR1.OC1M = 110: CNT < CCR1이면 High).
 *   가장자리 정렬 듀티 = CCR/(ARR+1), 중앙 정렬 듀티 = CCR/ARR. CCR=0이면 0 %, CCR>ARR이면 100 %.
 *   B1(PC13)을 누를 때마다 CCR이 0 → 250 → 500 → 999 → 1000 → 0 … 으로 바뀐다.
 *
 * 동기 시작 (마스터/슬레이브)
 *   TIM2 = 마스터: CR2.MMS = 001(Enable) → CEN이 켜지는 순간 TRGO가 High
 *   TIM3 = 슬레이브: SMCR.TS = 001(ITR1 = TIM2의 TRGO, RM0008 표 "TIMx internal trigger connection"),
 *                    SMCR.SMS = 110(트리거 모드: TRGI 상승 에지에서 하드웨어가 CEN을 켠다)
 *   → TIM2의 CEN 한 번으로 두 카운터가 같은 클럭에 함께 출발한다. 스코프에서 두 파형의 위상 관계를 비교하기 쉽다.
 *
 * 관찰 (AD2/오실로스코프 2채널: PA0, PA6)
 *   가장자리 정렬 펄스는 주기 시작(CNT=0)에 붙어 있고, 중앙 정렬 펄스는 카운터 골짜기(CNT=0)를 중심으로 대칭이다.
 *   CCR을 바꾸면 가장자리 정렬은 펄스의 뒤쪽 에지만, 중앙 정렬은 양쪽 에지가 함께 움직인다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2 → TIM2·TIM3 클럭 64 MHz
 *
 * 사용 레지스터·비트 (RM0008 15.4절)
 *   CR1  : CEN, ARPE(bit7), CMS[6:5]       CR2 : MMS[6:4]
 *   SMCR : SMS[2:0], TS[6:4]               EGR : UG
 *   CCMR1: OC1M[6:4] = 110, OC1PE(bit3)    CCER: CC1E(bit0), CC1P(bit1)
 *   PSC, ARR, CCR1
 *   CMS는 CEN = 0일 때만 바꾼다(가장자리 → 중앙 정렬 전환은 카운터 동작 중 금지).
 *
 * 인터럽트를 사용하지 않는다. 출력: USART2 115200 8N1.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define PWM_ARR     999u

static const uint16_t kCcr[] = { 0, 250, 500, 999, 1000 };

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트 스테이트 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;     /* HSI/2 × 16, APB1 ÷2 */
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
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 */
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
/* 정수부.소수 1자리 출력 (v10 = 값 × 10) */
static void uart_put_x10(uint32_t v10) { uart_putu(v10 / 10u); uart_putc('.'); uart_putc((char)('0' + v10 % 10u)); }

/* ---------------- 타이머 설정 ---------------- */

static void PWM_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;  /* 슬레이브(TIM3) 클럭도 트리거 전에 켜 둔다 */

    /* PA0 = TIM2_CH1, PA6 = TIM3_CH1: 대체기능 푸시풀 50 MHz (CNF=10, MODE=11 → 0xB). 리맵 없음 */
    GPIOA->CRL = (GPIOA->CRL & ~((0xFu << 0) | (0xFu << 24))) | (0xBu << 0) | (0xBu << 24);
    /* PA5 = LD2 출력, PC13 = B1 입력(리셋값 입력 플로팅, 보드 풀업) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);

    /* ---- TIM2: 마스터, 가장자리 정렬 ---- */
    TIM2->CR1   = TIM_CR1_ARPE;                         /* CMS = 00, DIR = 0(업) */
    TIM2->PSC   = 64 - 1;                               /* 1 MHz */
    TIM2->ARR   = PWM_ARR;
    TIM2->CCR1  = kCcr[1];
    TIM2->CCMR1 = (6u << 4) | TIM_CCMR1_OC1PE;          /* OC1M = 110 (PWM 모드 1), CCR1 프리로드, CC1S = 00(출력) */
    TIM2->CCER  = TIM_CCER_CC1E;                        /* CC1P = 0(능동 High), 핀 출력 */
    TIM2->CR2   = (1u << 4);                            /* MMS = 001: CNT_EN → TRGO */

    /* ---- TIM3: 슬레이브, 중앙 정렬 모드 1 ---- */
    TIM3->CR1   = TIM_CR1_ARPE | (1u << 5);             /* CMS = 01. CEN = 0인 지금 설정해야 한다 */
    TIM3->PSC   = 64 - 1;
    TIM3->ARR   = PWM_ARR;
    TIM3->CCR1  = kCcr[1];
    TIM3->CCMR1 = (6u << 4) | TIM_CCMR1_OC1PE;
    TIM3->CCER  = TIM_CCER_CC1E;
    TIM3->SMCR  = (1u << 4) | 6u;                       /* TS = 001(ITR1 = TIM2), SMS = 110(트리거 모드) */

    /* 프리로드 → 섀도 복사. 두 카운터 모두 0에서 출발 */
    TIM2->EGR = TIM_EGR_UG;
    TIM3->EGR = TIM_EGR_UG;

    TIM2->CR1 |= TIM_CR1_CEN;                           /* TIM2 시작 → TRGO ↑ → TIM3.CEN이 하드웨어로 켜진다 */
}

static void Report(uint32_t ccr)
{
    uint32_t edge10   = (ccr > PWM_ARR) ? 1000u : (ccr * 1000u) / (PWM_ARR + 1u);
    uint32_t center10 = (ccr > PWM_ARR) ? 1000u : (ccr * 1000u) / PWM_ARR;
    uart_puts("CCR1=");
    uart_putu(ccr);
    uart_puts("  edge(PA0) duty ");
    uart_put_x10(edge10);
    uart_puts("%  | center(PA6) duty ");
    uart_put_x10(center10);
    uart_puts("%   TIM3 CEN=");
    uart_putu(TIM3->CR1 & TIM_CR1_CEN);
    uart_puts(" DIR=");
    uart_putu((TIM3->CR1 & TIM_CR1_DIR) ? 1u : 0u);      /* 중앙 정렬에서 DIR은 하드웨어가 갱신(읽기 전용) */
    uart_puts("\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    UART2_Init();
    PWM_Init();

    uart_puts("\r\n15_TIM_CenterAligned_PWM_Reg_c  ARR=999, 1 count = 1 us\r\n");
    uart_puts("edge-aligned period = 1000 us (1 kHz), center-aligned period = 1998 us (500.5 Hz)\r\n");
    uart_puts("B1: CCR1 0 -> 250 -> 500 -> 999 -> 1000(>ARR)\r\n");

    uint32_t idx = 1;
    uint32_t prev = 1;                                  /* B1: 눌림 = Low */
    Report(kCcr[idx]);

    while (1)
    {
        uint32_t now = (GPIOC->IDR >> 13) & 1u;
        if (prev == 1u && now == 0u)                    /* 하강 에지 = 눌림 */
        {
            delay_us(30000);                            /* 30 ms 디바운스 */
            if (((GPIOC->IDR >> 13) & 1u) == 0u)
            {
                idx = (idx + 1u) % (sizeof kCcr / sizeof kCcr[0]);
                TIM2->CCR1 = kCcr[idx];                 /* OC1PE = 1: 다음 UEV부터 반영 */
                TIM3->CCR1 = kCcr[idx];
                GPIOA->ODR ^= (1u << 5);
                Report(kCcr[idx]);
            }
        }
        prev = now;
    }
}

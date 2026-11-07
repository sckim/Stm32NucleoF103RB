/**
 * 11_GPIO_BSRR_Race_Speed_Reg_c  —  [계층: 레지스터(CMSIS)]  ODR 읽기-수정-쓰기 경쟁 대 BSRR 원자적 쓰기, 출력 속도(MODE) 비교
 *
 * 교안 「GPIO·AFIO」 6.2절(ODR과 BSRR)·6.1절(출력 드라이버와 속도)의 실습이다.
 *
 * 실험 A — 같은 포트의 서로 다른 비트를 main과 ISR이 함께 바꿀 때
 *   main  : PB0을 계속 토글한다.
 *   ISR   : TIM2 갱신 인터럽트(100 kHz)마다 PB1을 토글한다.
 *   (1) ODR 방식 : 두 쪽 모두 GPIOB->ODR을 "읽고 → 비트를 바꾸고 → 쓴다".
 *       main이 ODR을 읽은 뒤 쓰기 전에 ISR이 끼어들어 PB1을 바꾸면, main이 옛 값을 다시 써서
 *       ISR의 PB1 변경을 덮어쓴다(갱신 유실). main은 쓰기 직후 PB1이 ISR이 마지막으로 쓴 값과 같은지 검사해 유실 횟수를 센다.
 *   (2) BSRR 방식 : 두 쪽 모두 GPIOB->BSRR에 set/reset 비트만 쓴다. 읽기가 없으므로 유실이 원리적으로 0이다.
 *   경쟁 창을 넓히려고 main의 읽기와 쓰기 사이에 짧은 지연(WINDOW_LOOPS)을 둔다.
 *
 * 실험 B — 출력 속도 설정(CRH.MODE8)과 에지
 *   PA8(Nucleo D7)을 MODE = 10(2 MHz) → 01(10 MHz) → 11(50 MHz) 순서로 바꾸며 각 2초씩 최대 속도로 토글한다.
 *   소프트웨어 토글 속도(DWT로 측정, UART 출력)는 세 경우 모두 같다. 달라지는 것은 상승·하강 에지의 기울기이다.
 *   데이터시트 표 38: 상승/하강 시간 최대 125 ns(2 MHz, 50 pF) / 25 ns(10 MHz, 50 pF) / 5 ns(50 MHz, 30 pF).
 *   AD2 또는 오실로스코프로 D7의 에지를 보고 비교한다. (배선이 없어도 실험 A는 그대로 동작한다.)
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2(32 MHz, TIM2 클럭 ×2 = 64 MHz), APB2 ÷1(64 MHz, GPIO)
 *
 * 사용 레지스터·비트
 *   RCC->APB2ENR : IOPAEN(bit2), IOPBEN(bit3)       RCC->APB1ENR : TIM2EN(bit0), USART2EN(bit17)
 *   GPIOB->CRL   : PB0·PB1 = CNF 00(범용 푸시풀) + MODE 11(50 MHz) → 니블 0x3
 *   GPIOA->CRH   : PA8 = CNF 00 + MODE 10/01/11 → 니블 0x2/0x1/0x3
 *   GPIOx->ODR   : 출력 데이터(읽기·쓰기, 워드 접근만 허용, RM0008 9.2.4)
 *   GPIOx->BSRR  : [15:0] BS 1이면 set, [31:16] BR 1이면 reset, 둘 다 1이면 BS 우선 (RM0008 9.2.5)
 *   TIM2->PSC/ARR/DIER.UIE/SR.UIF/CR1.CEN, DWT->CYCCNT
 *
 * ISR: TIM2_IRQHandler (벡터 테이블의 weak 심볼 재정의). HAL 콜백은 쓰지 않는다.
 *      ISR은 UIF를 지우고 PB1만 바꾼다. 출력은 모두 main 루프가 한다.
 * 출력: USART2(PA2, ST-LINK 가상 COM) 115200 8N1.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ    64000000u
#define PCLK1_HZ     32000000u
#define CYC_PER_US   (SYSCLK_HZ / 1000000u)
#define N_ITER       200000u     /* 실험 A 한 번에 main이 토글하는 횟수 */
#define WINDOW_LOOPS 8u          /* 읽기와 쓰기 사이 지연(경쟁 창) */

static volatile uint32_t g_use_bsrr;   /* 0: ODR 읽기-수정-쓰기, 1: BSRR */
static volatile uint32_t g_pb1;        /* ISR이 마지막으로 PB1에 쓴 값 */
static volatile uint32_t g_isr_count;

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트(LATENCY = 010) */
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
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX: AF 푸시풀 50 MHz(CNF 10, MODE 11) */
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

/* ---------------- ISR ---------------- */

/* 진짜 ISR. PB1을 토글한다. 방식은 main과 같게(ODR 읽기-수정-쓰기 또는 BSRR) 맞춘다. */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR = ~TIM_SR_UIF;                       /* rc_w0: 0을 써서 UIF만 지운다 */
        uint32_t v = g_pb1 ^ 1u;
        if (g_use_bsrr)
            GPIOB->BSRR = v ? GPIO_BSRR_BS1 : GPIO_BSRR_BR1;
        else
            GPIOB->ODR = (GPIOB->ODR & ~(1u << 1)) | (v << 1);
        g_pb1 = v;
        g_isr_count++;
    }
}

/* ---------------- 실험 A ---------------- */

static uint32_t Race_Run(uint32_t use_bsrr)
{
    uint32_t lost = 0, pb0 = 0;
    g_use_bsrr  = use_bsrr;
    g_pb1       = (GPIOB->ODR >> 1) & 1u;
    g_isr_count = 0;
    TIM2->CNT   = 0;
    TIM2->SR    = 0;
    TIM2->DIER  = TIM_DIER_UIE;
    TIM2->CR1  |= TIM_CR1_CEN;

    for (uint32_t i = 0; i < N_ITER; i++)
    {
        pb0 ^= 1u;
        if (use_bsrr)
        {
            GPIOB->BSRR = pb0 ? GPIO_BSRR_BS0 : GPIO_BSRR_BR0;   /* 읽기 없음: PB1은 건드리지 않는다 */
        }
        else
        {
            uint32_t odr = GPIOB->ODR;                           /* ① 읽기 */
            for (volatile uint32_t w = 0; w < WINDOW_LOOPS; w++) {}   /* 경쟁 창 */
            odr = (odr & ~1u) | pb0;                             /* ② 수정 */
            GPIOB->ODR = odr;                                    /* ③ 쓰기: 그 사이 ISR이 바꾼 PB1을 옛 값으로 되돌릴 수 있다 */
        }

        __disable_irq();                                         /* 검사와 복구 동안 ISR을 막는다 */
        if (((GPIOB->ODR >> 1) & 1u) != g_pb1)
        {
            lost++;
            GPIOB->BSRR = g_pb1 ? GPIO_BSRR_BS1 : GPIO_BSRR_BR1; /* 다음 반복을 위해 복구 */
        }
        __enable_irq();
    }

    TIM2->CR1  &= ~TIM_CR1_CEN;
    TIM2->DIER  = 0;
    return lost;
}

/* ---------------- 실험 B ---------------- */

static void Speed_Run(uint32_t mode, const char *name)
{
    GPIOA->CRH = (GPIOA->CRH & ~0xFu) | (mode & 0x3u);           /* PA8: CNF 00, MODE = mode */
    uint32_t n = 0;
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < 2000000u * CYC_PER_US)
    {
        GPIOA->BSRR = GPIO_BSRR_BS8;
        GPIOA->BSRR = GPIO_BSRR_BR8;
        n++;
    }
    uart_puts("  PA8(D7) MODE=");
    uart_puts(name);
    uart_puts(" : toggle rate ~");
    uart_putu(n / 2000u);                                        /* 2초 동안 주기 수 → kHz */
    uart_puts(" kHz (same for every MODE; compare edges on a scope)\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    UART2_Init();

    /* PB0, PB1: 범용 푸시풀 50 MHz (CRL[7:0] = 0x33) */
    GPIOB->CRL = (GPIOB->CRL & ~0xFFu) | 0x33u;
    /* PA5 = LD2: 범용 푸시풀 2 MHz (CRL[23:20] = 0x2) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);

    /* TIM2: 64 MHz / 64 = 1 MHz, ARR = 9 → 100 kHz 갱신 인터럽트 */
    TIM2->PSC = 64 - 1;
    TIM2->ARR = 10 - 1;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR  = 0;
    NVIC_SetPriority(TIM2_IRQn, 1);
    NVIC_EnableIRQ(TIM2_IRQn);

    uart_puts("\r\n11_GPIO_BSRR_Race_Speed_Reg_c  (SYSCLK 64 MHz, ISR 100 kHz on PB1, main toggles PB0)\r\n");

    for (uint32_t round = 1;; round++)
    {
        uart_puts("[round "); uart_putu(round); uart_puts("]\r\n");

        uint32_t lost_odr  = Race_Run(0);
        uint32_t isr_odr   = g_isr_count;
        uint32_t lost_bsrr = Race_Run(1);
        uint32_t isr_bsrr  = g_isr_count;
        uart_puts("  A) ODR read-modify-write : lost PB1 updates = "); uart_putu(lost_odr);
        uart_puts(" / ISR runs "); uart_putu(isr_odr); uart_puts("\r\n");
        uart_puts("     BSRR atomic write     : lost PB1 updates = "); uart_putu(lost_bsrr);
        uart_puts(" / ISR runs "); uart_putu(isr_bsrr); uart_puts("   (expect 0)\r\n");

        uart_puts("  B) output speed setting\r\n");
        Speed_Run(0x2u, "10 (2 MHz) ");
        Speed_Run(0x1u, "01 (10 MHz)");
        Speed_Run(0x3u, "11 (50 MHz)");

        GPIOA->BSRR = (GPIOA->ODR & (1u << 5)) ? GPIO_BSRR_BR5 : GPIO_BSRR_BS5;   /* LD2 라운드 표시 */
        delay_us(1000000u);
    }
}

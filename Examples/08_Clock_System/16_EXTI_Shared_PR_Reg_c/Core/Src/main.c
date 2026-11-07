/**
 * 16_EXTI_Shared_PR_Reg_c  —  [계층: 레지스터(CMSIS)]  EXTI 경로(AFIO→EXTI→NVIC), 공유 벡터 판별, PR 지우기 함정, 접점 튐 계수
 *
 * 교안 「인터럽트 체계(NVIC·EXTI·SysTick)」 7절(EXTI)의 실습이다. HAL 예제 01_GPIO/06_ExtInt·08_ExternalInt의 레지스터판이다.
 *
 * 경로
 *   PC13(B1) ─ AFIO_EXTICR4.EXTI13 = 0010(포트 C) ─ EXTI 라인 13 ─ FTSR13(하강 에지) ─ PR13 ─ IMR13 ─ NVIC EXTI15_10_IRQn(40)
 *   라인 10은 핀 없이 SWIER10(소프트웨어 트리거)으로만 쓴다. 라인 10~15는 벡터 하나(EXTI15_10)를 공유하므로
 *   ISR은 EXTI->PR을 읽어 어느 라인인지 판별해야 한다.
 *
 * 실험 (3초마다 자동 반복)
 *   A) 공유 벡터: SWIER10과 SWIER13을 PRIMASK 안에서 함께 세운 뒤 풀면 ISR은 몇 번 들어오는가?
 *      ISR이 대기 중인 라인을 모두 처리하면 1번에 두 라인을 처리한다.
 *   B) PR을 지우지 않으면: EXTI는 PR 비트가 1인 동안 NVIC로 요청을 계속 낸다. ISR이 PR을 지우지 않고 복귀하면
 *      곧바로 다시 들어온다. 100번 들어오면 ISR이 스스로 PR을 지우고 멈춘다.
 *   C) PR을 ISR "맨 끝"에서 지울 때: 지우기 쓰기가 반영되기 전에 예외 복귀가 끝나면 한 번 더 들어올 수 있다는 보고가 있다.
 *      ① 맨 앞에서 지우기 ② 맨 끝에서 지우기 ③ 맨 끝에서 지우고 PR을 다시 읽기(쓰기 완료를 기다림) 세 가지를
 *      각 200회 트리거해 "트리거 1회당 ISR 진입 수"를 센다. 원인은 매뉴얼로 단정하지 않고 이 실측으로 판단한다.
 *   D) B1을 한 번 누를 때 하강 에지가 몇 번 잡히는지(접점 튐) 50 ms 창으로 세어 출력한다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB2 ÷1 (EXTI·AFIO는 APB2), APB1 ÷2 (USART2)
 *
 * 사용 레지스터 (RM0008 9.4.6, 10.3)
 *   RCC->APB2ENR.AFIOEN, IOPCEN    : AFIO 클럭을 켜야 EXTICR에 쓸 수 있다
 *   AFIO->EXTICR[3] bits[7:4]      : EXTI13 포트 선택 (0000 PA ~ 0110 PG)
 *   EXTI->IMR, FTSR, RTSR, SWIER, PR : 인터럽트 마스크, 하강/상승 에지, 소프트웨어 트리거, 대기(rc_w1: 1을 써서 지움)
 *   NVIC->ISER, NVIC_SetPriority(EXTI15_10_IRQn)
 *
 * ISR: EXTI15_10_IRQHandler (벡터 테이블의 weak 심볼 재정의). HAL 콜백은 없다. 출력은 모두 main 루프가 한다.
 * 출력: USART2 115200 8N1. 배선 없음(보드의 B1 사용).
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)

#define L10  (1u << 10)
#define L13  (1u << 13)

enum { MODE_NORMAL = 0, MODE_NOCLEAR, MODE_CLR_FIRST, MODE_CLR_LAST, MODE_CLR_LAST_READBACK };

static volatile uint32_t g_mode;
static volatile uint32_t g_entries;          /* ISR 진입 횟수 */
static volatile uint32_t g_hits10, g_hits13; /* 라인별 처리 횟수 */
static volatile uint32_t g_b1_edges;         /* 실험 D: 하강 에지 수 */
static volatile uint32_t g_b1_first;         /* 실험 D: 첫 에지 시각 */

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

/* ---------------- ISR ---------------- */

/* 진짜 ISR. 라인 10~15가 공유하는 벡터이므로 PR을 읽어 라인을 판별한다. */
void EXTI15_10_IRQHandler(void)
{
    uint32_t pr = EXTI->PR & (L10 | L13);      /* 이 ISR이 맡은 라인 중 대기 중인 것 */
    g_entries++;

    switch (g_mode)
    {
    case MODE_NOCLEAR:                         /* 실험 B: 지우지 않는다 → 곧바로 재진입 */
        if (g_entries >= 100u) EXTI->PR = pr;  /* 안전장치: 100번이면 지우고 끝낸다 */
        return;
    case MODE_CLR_FIRST:                       /* 실험 C①: 먼저 지우고 일한다 */
        EXTI->PR = pr;
        if (pr & L10) g_hits10++;
        return;
    case MODE_CLR_LAST:                        /* 실험 C②: 일한 뒤 맨 끝에서 지운다 */
        if (pr & L10) g_hits10++;
        EXTI->PR = pr;
        return;
    case MODE_CLR_LAST_READBACK:               /* 실험 C③: 맨 끝에서 지우고, 다시 읽어 쓰기 완료를 기다린다 */
        if (pr & L10) g_hits10++;
        EXTI->PR = pr;
        (void)EXTI->PR;
        return;
    default:                                   /* 실험 A·D: 정상 처리 */
        EXTI->PR = pr;                         /* rc_w1: 1을 쓴 비트만 지워진다 */
        if (pr & L10) g_hits10++;
        if (pr & L13)
        {
            g_hits13++;
            if (g_b1_edges == 0u) g_b1_first = DWT->CYCCNT;
            g_b1_edges++;
        }
        return;
    }
}

/* ---------------- 설정 ---------------- */

static void EXTI_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN;
    /* PC13 = 입력 플로팅(리셋값 0x4). B1은 보드에서 풀업되어 있어 누르면 Low */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x4u << 20);
    /* AFIO_EXTICR4: EXTI12[3:0] | EXTI13[7:4] | ... → EXTI13 = 0010(PC). EXTI10(EXTICR3[11:8])은 0000(PA10)이지만 에지를 켜지 않는다 */
    AFIO->EXTICR[3] = (AFIO->EXTICR[3] & ~(0xFu << 4)) | (0x2u << 4);

    EXTI->FTSR |= L13;                         /* 라인 13: 하강 에지 */
    EXTI->RTSR &= ~(L10 | L13);
    EXTI->FTSR &= ~L10;                        /* 라인 10: 에지 없음 → SWIER로만 대기 */
    EXTI->PR    = L10 | L13;                   /* 남은 대기 정리 */
    EXTI->IMR  |= L10 | L13;                   /* 인터럽트 마스크 해제 */

    NVIC_SetPriority(EXTI15_10_IRQn, 2);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* ---------------- 실험 ---------------- */

static void ExpA_Shared(void)
{
    g_mode = MODE_NORMAL;
    g_entries = g_hits10 = g_hits13 = 0;
    __disable_irq();
    EXTI->IMR &= ~L13;                         /* B1 영향 배제 */
    EXTI->SWIER = L10 | L13;                   /* 두 라인을 소프트웨어로 대기: PR10, PR13 = 1 */
    __enable_irq();
    delay_us(100);
    EXTI->PR = L13;
    EXTI->IMR |= L13;
    uart_puts("A) SWIER10+SWIER13 together : ISR entries = ");
    uart_putu(g_entries);
    uart_puts(" , line10 handled = "); uart_putu(g_hits10);
    uart_puts("   (expect 1 entry; IMR13 was off so PR13 alone did not request)\r\n");

    /* 같은 실험을 IMR13을 켠 채로 */
    g_entries = g_hits10 = g_hits13 = 0;
    g_b1_edges = 0;
    __disable_irq();
    EXTI->SWIER = L10 | L13;
    __enable_irq();
    delay_us(100);
    g_b1_edges = 0;                            /* 소프트웨어 트리거는 B1 계수에서 뺀다 */
    uart_puts("   with IMR13 on            : ISR entries = ");
    uart_putu(g_entries);
    uart_puts(" , line10 = "); uart_putu(g_hits10);
    uart_puts(" , line13 = "); uart_putu(g_hits13);
    uart_puts("   (expect 1 entry handling both lines)\r\n");
}

static void ExpB_NoClear(void)
{
    g_mode = MODE_NOCLEAR;
    g_entries = 0;
    EXTI->SWIER = L10;
    delay_us(2000);
    g_mode = MODE_NORMAL;
    uart_puts("B) PR10 never cleared       : ISR entries for 1 trigger = ");
    uart_putu(g_entries);
    uart_puts("   (stopped by guard at 100)\r\n");
}

static void ExpC_ClearTiming(uint32_t mode, const char *name)
{
    g_mode = mode;
    g_entries = g_hits10 = 0;
    for (int k = 0; k < 200; k++)
    {
        EXTI->SWIER = L10;
        delay_us(20);
    }
    g_mode = MODE_NORMAL;
    uart_puts("   ");
    uart_puts(name);
    uart_puts(" : 200 triggers -> ISR entries = ");
    uart_putu(g_entries);
    uart_puts(" , extra = ");
    uart_putu(g_entries - 200u);
    uart_puts("\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */
    EXTI_Init();

    uart_puts("\r\n16_EXTI_Shared_PR_Reg_c  (PC13=B1 on EXTI13, EXTI10 by SWIER, shared vector EXTI15_10)\r\n");
    uart_puts("press B1 any time: falling edges per press are counted (contact bounce)\r\n");

    uint32_t t_next = DWT->CYCCNT;
    while (1)
    {
        /* 실험 D: 첫 에지 후 50 ms가 지나면 그 사이 잡힌 하강 에지 수를 출력 */
        if (g_b1_edges && (DWT->CYCCNT - g_b1_first) > 50000u * CYC_PER_US)
        {
            uart_puts("D) B1 press: falling edges in 50 ms = ");
            uart_putu(g_b1_edges);
            uart_puts(g_b1_edges > 1u ? "  (bounce!)\r\n" : "\r\n");
            g_b1_edges = 0;
            GPIOA->ODR ^= (1u << 5);
        }

        if ((int32_t)(DWT->CYCCNT - t_next) >= 0)
        {
            t_next += 3000000u * CYC_PER_US;
            uart_puts("\r\n");
            ExpA_Shared();
            ExpB_NoClear();
            uart_puts("C) when PR is cleared inside the ISR\r\n");
            ExpC_ClearTiming(MODE_CLR_FIRST, "clear first          ");
            ExpC_ClearTiming(MODE_CLR_LAST, "clear last           ");
            ExpC_ClearTiming(MODE_CLR_LAST_READBACK, "clear last + readback");
        }
    }
}

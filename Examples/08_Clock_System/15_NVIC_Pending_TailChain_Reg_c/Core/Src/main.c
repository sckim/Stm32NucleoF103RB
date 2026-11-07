/**
 * 15_NVIC_Pending_TailChain_Reg_c  —  [계층: 레지스터(CMSIS)]  NVIC 대기·우선순위·선점·테일 체이닝·BASEPRI를 순서 기록으로 확인
 *
 * 교안 「인터럽트 체계(NVIC·EXTI·SysTick)」 4~6절의 실습이다.
 *
 * 무엇을 하나
 *   주변장치 없이 NVIC_ISPR(인터럽트 set-pending 레지스터)에 직접 써서 IRQ 세 개를 "대기(pending)" 상태로 만든다.
 *     IRQ7 = EXTI1_IRQn, IRQ8 = EXTI2_IRQn, IRQ9 = EXTI3_IRQn  (이 예제에서는 EXTI 주변장치를 설정하지 않는다.
 *     NVIC는 요청이 어디서 왔는지 구분하지 않으므로 소프트웨어 대기만으로 ISR이 실행된다.)
 *   각 ISR은 진입·탈출 시각(DWT->CYCCNT, 64 MHz 사이클)을 기록하고, main이 순서와 간격을 출력한다.
 *
 * 실험 (PM0056 2.3.5~2.3.7, 4.3)
 *   1) 진입 지연: Thread 모드에서 ISPR에 쓴 순간부터 ISR 첫 줄까지의 사이클.
 *      Cortex-M3의 최소 진입 지연은 무대기 메모리 기준 12사이클로 알려져 있다(Definitive Guide 2판 9.7절).
 *      64 MHz에서는 Flash 2 웨이트 스테이트라 이보다 길게 나올 수 있다.
 *   2) 같은 우선순위 세 개를 동시에 대기 → IRQ 번호가 작은 것부터 실행, 사이에 스택 복원·재저장 없이 이어짐(테일 체이닝).
 *   3) 우선순위가 다르면 번호와 무관하게 우선순위 값이 작은 것부터 실행.
 *   4) 선점과 중첩: 낮은 우선순위 ISR 안에서 높은 우선순위 IRQ를 대기시키면 즉시 끼어든다(중첩).
 *      같은 우선순위 IRQ를 대기시키면 끼어들지 못하고, 현재 ISR이 끝난 뒤 테일 체이닝으로 실행된다.
 *   5) BASEPRI: 값 이상(우선순위가 같거나 낮은)의 예외를 막는다. 막힌 IRQ는 대기 상태로 남아 있다가 BASEPRI를 풀면 실행된다.
 *   6) 서브 우선순위: AIRCR.PRIGROUP = 5(선점 2비트 + 서브 2비트)에서 선점 우선순위가 같으면 서브가 달라도 선점하지 못한다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2 (USART2 = PCLK1 32 MHz)
 *
 * 사용 레지스터 (PM0056 4.3, 4.4)
 *   NVIC->ISER[0], ISPR[0], ICPR[0], IABR[0], IP[n]   : 허용, 대기 설정·해제, 활성 상태, 우선순위(상위 4비트만 구현)
 *   SCB->AIRCR.PRIGROUP[10:8] (VECTKEY 0x5FA와 함께 써야 함) : 선점/서브 분할. CMSIS NVIC_SetPriorityGrouping()
 *   PRIMASK(__disable_irq/__enable_irq), BASEPRI(__set_BASEPRI)
 *   DWT->CYCCNT : 사이클 카운터
 *
 * ISR: EXTI1_IRQHandler / EXTI2_IRQHandler / EXTI3_IRQHandler (벡터 테이블의 weak 심볼 재정의). HAL 콜백은 없다.
 *      ISR은 기록만 하고, 시나리오에 따라 다른 IRQ를 대기시킨다. 출력은 모두 main 루프가 한다.
 * 출력: USART2(PA2, ST-LINK 가상 COM) 115200 8N1. 배선 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)

#define IRQ_A  EXTI1_IRQn   /* 7 */
#define IRQ_B  EXTI2_IRQn   /* 8 */
#define IRQ_C  EXTI3_IRQn   /* 9 */

typedef struct { uint8_t irq; char ev; uint32_t cyc; } LogEntry;

#define LOG_N 32u
static volatile LogEntry g_log[LOG_N];
static volatile uint32_t g_nlog;
static volatile uint32_t g_scenario;            /* ISR 동작 선택 */
static volatile uint32_t g_t_pend;              /* 실험 1: ISPR에 쓴 시각 */
static volatile uint32_t g_t_entry;             /* 실험 1: ISR 첫 줄 시각 */

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 48 < SYSCLK ≤ 72 MHz: 2 웨이트 스테이트 */
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
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
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

/* ---------------- 기록 ---------------- */

static inline void Log(uint8_t irq, char ev)
{
    uint32_t n = g_nlog;
    if (n < LOG_N) { g_log[n].irq = irq; g_log[n].ev = ev; g_log[n].cyc = DWT->CYCCNT; g_nlog = n + 1u; }
}

static inline void Pend(IRQn_Type irq) { NVIC->ISPR[0] = 1u << (uint32_t)irq; }   /* 쓰기 1 = 대기 설정 */

/* ---------------- ISR (진짜 ISR, 콜백 없음) ---------------- */

void EXTI1_IRQHandler(void)       /* IRQ7 */
{
    if (g_scenario == 1u) { g_t_entry = DWT->CYCCNT; return; }
    Log(7, 'E');
    if (g_scenario == 4u)
    {
        Pend(IRQ_C);              /* 우선순위 1(높음): 즉시 선점해야 한다 */
        __DSB(); __ISB();         /* 대기 설정이 NVIC에 반영된 뒤 다음 명령을 실행 */
        Pend(IRQ_B);              /* 우선순위 3(같음): 선점하지 못하고 기다린다 */
        __DSB(); __ISB();
        Log(7, 'm');              /* IRQ7 본문 중간 표시 */
    }
    if (g_scenario == 6u)
    {
        Pend(IRQ_B);              /* 선점 우선순위 같고 서브만 높음: 선점 불가 */
        __DSB(); __ISB();
        Log(7, 'm');
    }
    Log(7, 'X');
}

void EXTI2_IRQHandler(void)       /* IRQ8 */
{
    Log(8, 'E');
    Log(8, 'X');
}

void EXTI3_IRQHandler(void)       /* IRQ9 */
{
    Log(9, 'E');
    Log(9, 'X');
}

/* ---------------- 출력 ---------------- */

static void PrintLog(const char *title, const char *expect)
{
    uart_puts(title);
    uart_puts("\r\n   order : ");
    for (uint32_t i = 0; i < g_nlog; i++)
    {
        uart_putu(g_log[i].irq);
        uart_putc(g_log[i].ev);
        uart_putc(' ');
    }
    uart_puts("\r\n   expect: ");
    uart_puts(expect);
    /* 테일 체이닝 간격: 한 ISR의 X 기록과 다음 ISR의 E 기록 사이 사이클 */
    uart_puts("\r\n   X->E gap [cycles]:");
    for (uint32_t i = 1; i < g_nlog; i++)
    {
        if (g_log[i - 1].ev == 'X' && g_log[i].ev == 'E')
        {
            uart_putc(' ');
            uart_putu(g_log[i].cyc - g_log[i - 1].cyc);
        }
    }
    uart_puts("\r\n");
}

static void SetPrio(uint32_t a, uint32_t b, uint32_t c)
{
    NVIC_SetPriority(IRQ_A, a);   /* NVIC->IP[7] = a << 4 */
    NVIC_SetPriority(IRQ_B, b);
    NVIC_SetPriority(IRQ_C, c);
}

static void Reset(uint32_t scenario)
{
    __disable_irq();
    NVIC->ICPR[0] = (1u << IRQ_A) | (1u << IRQ_B) | (1u << IRQ_C);  /* 남은 대기 해제 */
    g_nlog = 0;
    g_scenario = scenario;
}

/* ---------------- 실험 ---------------- */

static void Exp1_Latency(void)
{
    uint32_t mn = 0xFFFFFFFFu, mx = 0;
    SetPrio(2, 2, 2);
    for (int k = 0; k < 16; k++)
    {
        Reset(1);
        __enable_irq();
        g_t_entry = 0;
        g_t_pend = DWT->CYCCNT;
        Pend(IRQ_A);                              /* 대기 → 우선순위 충분 → 즉시 진입 */
        while (g_t_entry == 0) {}
        uint32_t d = g_t_entry - g_t_pend;
        if (d < mn) mn = d;
        if (d > mx) mx = d;
    }
    uart_puts("1) ISPR write -> first ISR line [cycles @64MHz]: min ");
    uart_putu(mn);
    uart_puts(" , max ");
    uart_putu(mx);
    uart_puts("   (zero-wait Cortex-M3 minimum is 12; Flash 2WS adds more)\r\n");
}

static void Exp2_SamePriority(void)
{
    SetPrio(2, 2, 2);
    Reset(2);
    Pend(IRQ_C); Pend(IRQ_B); Pend(IRQ_A);        /* 일부러 9 → 8 → 7 순으로 대기 */
    __enable_irq();                               /* PRIMASK 해제: 세 개가 동시에 대기 중 */
    delay_us(50);
    PrintLog("2) same priority 2/2/2, pended 9,8,7 under PRIMASK", "7E 7X 8E 8X 9E 9X (lowest IRQ number first, tail-chained)");
}

static void Exp3_DifferentPriority(void)
{
    SetPrio(3, 2, 1);                             /* IRQ9가 가장 높다 */
    Reset(3);
    Pend(IRQ_A); Pend(IRQ_B); Pend(IRQ_C);
    __enable_irq();
    delay_us(50);
    PrintLog("3) priority 7:3, 8:2, 9:1, pended 7,8,9 under PRIMASK", "9E 9X 8E 8X 7E 7X (priority value wins over IRQ number)");
}

static void Exp4_Preemption(void)
{
    SetPrio(3, 3, 1);
    Reset(4);
    __enable_irq();
    Pend(IRQ_A);                                  /* IRQ7 ISR이 9(높음)와 8(같음)을 대기시킨다 */
    delay_us(50);
    PrintLog("4) in IRQ7 (prio 3): pend IRQ9 (prio 1), then IRQ8 (prio 3)", "7E 9E 9X 7m 7X 8E 8X (9 nests into 7; 8 waits, then tail-chains)");
}

static void Exp5_Basepri(void)
{
    SetPrio(3, 2, 1);
    Reset(5);
    __set_BASEPRI(2u << 4);                       /* 우선순위 값 2 이상(2, 3)은 막는다 */
    Pend(IRQ_A); Pend(IRQ_B); Pend(IRQ_C);
    __enable_irq();
    delay_us(50);
    uint32_t n_blocked = g_nlog;
    uint32_t ispr = NVIC->ISPR[0] & ((1u << IRQ_A) | (1u << IRQ_B) | (1u << IRQ_C));
    __set_BASEPRI(0);                             /* 해제 → 막혀 있던 대기가 실행된다 */
    delay_us(50);
    PrintLog("5) BASEPRI = 0x20, pend 7(3) 8(2) 9(1)", "9E 9X | then after BASEPRI=0: 8E 8X 7E 7X");
    uart_puts("   while BASEPRI set: entries logged = ");
    uart_putu(n_blocked);
    uart_puts(" , still pending (ISPR bits 7..9) = 0b");
    uart_putc((ispr >> IRQ_C) & 1u ? '1' : '0');
    uart_putc((ispr >> IRQ_B) & 1u ? '1' : '0');
    uart_putc((ispr >> IRQ_A) & 1u ? '1' : '0');
    uart_puts("  (expect 0b011: 8 and 7 waiting)\r\n");
}

static void Exp6_SubPriority(void)
{
    NVIC_SetPriorityGrouping(5);                  /* AIRCR.PRIGROUP = 5: [7:6] 선점 2비트, [5:4] 서브 2비트 */
    uint32_t pg = NVIC_GetPriorityGrouping();
    NVIC_SetPriority(IRQ_A, NVIC_EncodePriority(pg, 1, 3));   /* 선점 1, 서브 3 */
    NVIC_SetPriority(IRQ_B, NVIC_EncodePriority(pg, 1, 0));   /* 선점 1, 서브 0 (서브만 더 높음) */
    NVIC_SetPriority(IRQ_C, NVIC_EncodePriority(pg, 3, 3));
    Reset(6);
    __enable_irq();
    Pend(IRQ_A);
    delay_us(50);
    PrintLog("6) PRIGROUP=5: IRQ7 (pre 1, sub 3) pends IRQ8 (pre 1, sub 0)", "7E 7m 7X 8E 8X (same preempt group: sub-priority cannot preempt)");
    uart_puts("   IP[7]=0x");
    uart_putu(NVIC->IP[IRQ_A] >> 4); uart_puts("0 , IP[8]=0x"); uart_putu(NVIC->IP[IRQ_B] >> 4);
    uart_puts("0  (upper nibble = pre:sub = 01:11 / 01:00)\r\n");
    NVIC_SetPriorityGrouping(3);                  /* 원래대로: PRIGROUP = 3, 4비트 모두 선점 (HAL의 NVIC_PRIORITYGROUP_4와 같다) */
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */

    NVIC_SetPriorityGrouping(3);
    NVIC->ISER[0] = (1u << IRQ_A) | (1u << IRQ_B) | (1u << IRQ_C);  /* 허용 */

    uart_puts("\r\n15_NVIC_Pending_TailChain_Reg_c  (IRQ7=EXTI1, IRQ8=EXTI2, IRQ9=EXTI3 pended by software)\r\n");

    for (uint32_t round = 1;; round++)
    {
        uart_puts("\r\n[round "); uart_putu(round); uart_puts("]\r\n");
        Exp1_Latency();
        Exp2_SamePriority();
        Exp3_DifferentPriority();
        Exp4_Preemption();
        Exp5_Basepri();
        Exp6_SubPriority();
        __enable_irq();
        GPIOA->ODR ^= (1u << 5);
        delay_us(5000000u);
    }
}

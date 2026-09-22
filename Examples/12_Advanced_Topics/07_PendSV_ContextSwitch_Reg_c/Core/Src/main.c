/**
 * 07_PendSV_ContextSwitch_Reg_c  —  [계층: 레지스터(CMSIS) + 어셈블리]  미니 선점형 RTOS: SVC / PendSV / PSP
 *
 * FreeRTOS 같은 RTOS 가 "여러 작업을 동시에 실행하는 것처럼" 보이게 하는 원리를 최소 코드(약 150줄)로 구현한다.
 * (프레임워크에 RTOS 미포함 -> 원리 학습용. 실제 제품에는 검증된 RTOS 를 쓸 것)
 *
 * 구성
 *   작업 A : LD2(PA5) 를 500ms 마다 토글
 *   작업 B : 1초마다 UART(115200)로 "task B, tick=..., switches=..." 출력
 *   두 작업은 각자 자기 스택을 가지며, 10ms 마다 강제로 교대(시분할, 라운드로빈)한다. 작업은 delay 중에 바쁜 대기만 한다.
 *
 * Cortex-M3 예외/스택 구조
 *   MSP : 예외(ISR) 와 커널이 쓰는 메인 스택,  PSP : 작업(스레드)이 쓰는 프로세스 스택.  EXC_RETURN=0xFFFFFFFD -> 스레드 모드 + PSP 로 복귀
 *   예외 진입 시 하드웨어가 현재 스택에 R0-R3, R12, LR, PC, xPSR 을 자동 push, 복귀 시 자동 pop.
 *   -> 소프트웨어가 R4-R11 만 추가로 저장/복원하면 "작업 전체 문맥" 이 완성된다.
 *
 * 동작 순서
 *   1) 각 작업의 스택에 "예외에서 막 복귀하려는 것처럼 보이는" 가짜 프레임을 만들어 둔다 (stack_init).
 *   2) SVC 명령(svc 0) -> SVC_Handler: 첫 작업의 프레임을 PSP 로 복원하고 EXC_RETURN 으로 스레드 모드 진입
 *   3) SysTick(1ms) 이 tick 을 세고 10ms 마다 SCB->ICSR.PENDSVSET 으로 PendSV 를 "보류(pend)"
 *   4) PendSV(우선순위 최하) 핸들러: 현재 작업 R4-R11 저장 -> 다음 작업 선택 -> 그 작업의 R4-R11 복원 -> 복귀
 *      PendSV 를 최하위 우선순위로 두는 이유: 다른 인터럽트를 방해하지 않고 "모든 ISR 이 끝난 뒤" 문맥 교환을 하기 위해.
 *
 * 핵심 레지스터
 *   SCB->ICSR.PENDSVSET(bit28) : PendSV 보류,   SCB->SHP[10] : PendSV 우선순위 (0xF0 = 최하위),   SysTick->LOAD/CTRL : 1ms 틱
 *
 * ISR: SVC_Handler / PendSV_Handler / SysTick_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

/* ---------------- USART2 (작업 B 전용, 레지스터 구동) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);     /* PA2 AF-PP 50MHz, PA3 플로팅 입력 */
    USART2->BRR = (69u << 4) | 7u;                                /* 8MHz / 115200 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc(b[--i]);
}

/* ---------------- 커널 ---------------- */
#define N_TASKS     2
#define STACK_WORDS 128                                           /* 작업당 스택 512바이트 */

typedef struct { uint32_t *sp; } tcb_t;                          /* sp: 저장된 PSP (R4-R11 저장 후의 위치) */

static uint32_t stacks[N_TASKS][STACK_WORDS] __attribute__((aligned(8)));   /* 8바이트 정렬 필수 (AAPCS) */
static tcb_t tcb[N_TASKS];
static volatile uint32_t cur;                                     /* 현재 실행 중인 작업 번호 */
static volatile uint32_t g_tick;                                  /* 1ms 틱 */
static volatile uint32_t g_switches;                              /* 문맥 교환 횟수 */

static void task_exit(void) { while (1) {} }                      /* 작업 함수가 return 했을 때 도달(LR 자리) */

/* 스택 위쪽부터 채워 "예외 복귀 직전" 모양을 만든다.  반환값 = R4 가 놓인 최하단 주소(= 초기 PSP) */
static uint32_t *stack_init(uint32_t *stack_top, void (*fn)(void))
{
    uint32_t *sp = stack_top;
    *(--sp) = 0x01000000u;                 /* xPSR : T 비트(bit24)=1 (Thumb 상태 필수) */
    *(--sp) = (uint32_t)fn;                /* PC   : 작업 진입 주소 */
    *(--sp) = (uint32_t)task_exit;         /* LR   */
    *(--sp) = 0;                           /* R12  */
    *(--sp) = 0;                           /* R3   */
    *(--sp) = 0;                           /* R2   */
    *(--sp) = 0;                           /* R1   */
    *(--sp) = 0;                           /* R0   */
    for (int i = 0; i < 8; i++) *(--sp) = 0;   /* R11..R4 : 소프트웨어 저장 영역 */
    return sp;
}

/* PendSV 어셈블리가 호출하는 C 함수: 현재 PSP 를 저장하고 다음 작업의 PSP 를 돌려준다 (라운드로빈) */
__attribute__((used)) uint32_t *sched_switch(uint32_t *sp)
{
    tcb[cur].sp = sp;
    cur = (cur + 1u) % N_TASKS;
    g_switches++;
    return tcb[cur].sp;
}

__attribute__((used)) uint32_t *sched_first(void)
{
    cur = 0;
    return tcb[0].sp;
}

/* SVC: 첫 작업 시작. 이후로는 PendSV 만 사용한다 */
__attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile(
        "push {lr}              \n"
        "bl sched_first         \n"      /* r0 = 첫 작업의 저장된 SP */
        "pop {lr}               \n"
        "ldmia r0!, {r4-r11}    \n"      /* 소프트웨어 저장분 복원 */
        "msr psp, r0            \n"      /* 남은 8워드(하드웨어 프레임)가 PSP 가 가리키는 곳 */
        "mvn lr, #2             \n"      /* lr = 0xFFFFFFFD : 스레드 모드, PSP 로 복귀 */
        "bx lr                  \n");
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile(
        "mrs r0, psp            \n"      /* 현재 작업의 PSP (하드웨어 프레임 바로 아래) */
        "stmdb r0!, {r4-r11}    \n"      /* R4-R11 을 그 스택에 저장 */
        "push {lr}              \n"      /* 커널(MSP) 에 EXC_RETURN 보관 */
        "bl sched_switch        \n"      /* r0 = 다음 작업의 SP */
        "pop {lr}               \n"
        "ldmia r0!, {r4-r11}    \n"      /* 다음 작업 R4-R11 복원 */
        "msr psp, r0            \n"
        "bx lr                  \n");    /* 하드웨어가 나머지 8워드를 자동 pop 하고 작업으로 복귀 */
}

void SysTick_Handler(void)
{
    g_tick++;
    if ((g_tick % 10u) == 0u) SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;   /* 10ms 마다 문맥 교환 요청 */
}

/* ---------------- 작업 ---------------- */
static void delay_ms(uint32_t ms)
{
    uint32_t t = g_tick;
    while ((uint32_t)(g_tick - t) < ms) {}       /* 바쁜 대기: 선점 덕분에 다른 작업이 그동안 실행된다 */
}

static void task_a(void)
{
    while (1)
    {
        GPIOA->ODR ^= (1u << 5);                 /* LD2 토글 */
        delay_ms(500);
    }
}

static void task_b(void)
{
    while (1)
    {
        uart_puts("task B: tick=");    uart_dec(g_tick);
        uart_puts("ms switches=");     uart_dec(g_switches);
        uart_puts("\r\n");
        delay_ms(1000);
    }
}

int main(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);       /* PA5 출력 */
    uart_init();

    tcb[0].sp = stack_init(&stacks[0][STACK_WORDS], task_a);
    tcb[1].sp = stack_init(&stacks[1][STACK_WORDS], task_b);

    NVIC_SetPriority(PendSV_IRQn, (1u << __NVIC_PRIO_BITS) - 1u);   /* 최하위 우선순위 (0xF0) */
    NVIC_SetPriority(SysTick_IRQn, 0);
    SysTick->LOAD = 8000000u / 1000u - 1u;                          /* HSI 8MHz -> 1ms */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    uart_puts("\r\n[mini RTOS] starting tasks via SVC\r\n");
    __asm volatile("svc 0");                                        /* 첫 작업으로 진입 (돌아오지 않는다) */
    while (1) {}
}

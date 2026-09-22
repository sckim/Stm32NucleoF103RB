/**
 * 03_ExceptionModel_VectorTable_Reg_c  —  [계층: 레지스터(CMSIS) + 어셈블리]  벡터 테이블과 예외 진입/복귀의 최소 사례
 *
 * 지금까지 인터럽트를 "HAL이 알아서 처리해 주는 것"으로 썼다면, 이 예제는 그 안에서 실제로 무슨 일이 일어나는지 본다.
 * 06_NVIC_Priority(우선순위·선점 실험)와 08_HardFault_Diagnosis(폴트 스택 해석)의 **개념적 준비 단계**에 해당한다.
 *
 * 1) 벡터 테이블 (PM0056 2.3.4 Vector table, Figure 12)
 *    주소 0(리셋 후 `SCB->VTOR`=0 이면 Flash 시작에 별칭됨)에는 "함수 주소들의 배열"이 있다.
 *    슬롯0 = 리셋 직후의 초기 SP 값(함수 주소 아님!), 슬롯1 = Reset_Handler, 슬롯2 = NMI, 슬롯3 = HardFault, ...
 *    슬롯11 = SVCall, 슬롯14 = PendSV, 슬롯15 = SysTick, 슬롯16 부터 주변장치별 IRQ(USART2, TIM2 등)가 순서대로 이어진다.
 *    CPU 는 인터럽트가 나면 이 배열에서 해당 슬롯의 주소를 읽어 그리로 분기할 뿐, 마법이 아니다.
 *
 * 2) 예외 진입 시 자동 스택 저장 (PM0056 2.3.7 Exception entry and return)
 *    인터럽트가 나면 하드웨어가 **소프트웨어 개입 없이** R0-R3, R12, LR, PC(복귀 주소), xPSR 8워드를 현재 스택에 쌓는다.
 *    이 예제는 SysTick 인터럽트(1ms)의 첫 번째 진입에서 이 8워드를 그대로 읽어 UART 로 보여준다.
 *    (08_HardFault_Diagnosis 는 같은 8워드를 "폴트가 났을 때" 해석하는 예제 — 이 예제가 그 사전 지식이다)
 *
 * 3) EXC_RETURN (PM0056 2.3.7, Table 17)
 *    예외 진입 시 하드웨어는 LR 에 **일반 복귀 주소가 아니라** 0xFFFFFFF9 같은 특수 값(EXC_RETURN)을 넣는다.
 *    이후 `BX LR`(또는 `POP {PC}`)로 이 값을 PC 에 쓰면, 그 자체가 "예외에서 복귀하라"는 신호로 해석된다.
 *    0xFFFFFFF1=핸들러 모드로 복귀, 0xFFFFFFF9=스레드 모드+MSP 로 복귀, 0xFFFFFFFD=스레드 모드+PSP 로 복귀(RTOS).
 *    SVC(소프트웨어 인터럽트, `svc 0` 명령)를 직접 발생시켜 이 값을 확인한다.
 *
 * 4) 우선순위는 그냥 레지스터의 숫자다 — `NVIC_GetPriority()` 로 SysTick 의 현재 우선순위를 읽어 확인.
 *    "우선순위가 왜/어떻게 실행 순서를 바꾸는가"는 06_NVIC_Priority_HAL_c 에서 실험한다.
 *
 * ISR: SysTick_Handler, SVC_Handler (아래).
 */
#include "stm32f1xx.h"
#include <stdint.h>

static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);
    USART2->BRR = (69u << 4) | 7u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_hex(uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(hx[(v >> i) & 0xF]);
}
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc(b[--i]);
}

/* ---------------- 1) 벡터 테이블 덤프 ---------------- */
static const char *const vec_name[16] = {
    "0  Initial SP (함수 아님)", "1  Reset", "2  NMI", "3  HardFault", "4  MemManage",
    "5  BusFault", "6  UsageFault", "7  (reserved)", "8  (reserved)", "9  (reserved)",
    "10 (reserved)", "11 SVCall", "12 DebugMon", "13 (reserved)", "14 PendSV", "15 SysTick",
};

static void dump_vector_table(void)
{
    uint32_t base = SCB->VTOR;                     /* 0 이면 주소 0(Flash 로 별칭)에서 읽는다 */
    const uint32_t *vt = (const uint32_t *)base;
    uart_puts("\r\n[1] vector table @ "); uart_hex(base); uart_puts("\r\n");
    for (int i = 0; i < 16; i++)
    {
        uart_puts("  ["); uart_puts(vec_name[i]); uart_puts("]\t= "); uart_hex(vt[i]); uart_puts("\r\n");
    }
    uart_puts("  slot 16 (첫 주변장치 IRQ, WWDG) = "); uart_hex(vt[16]); uart_puts("\r\n");
}

/* ---------------- 2) SysTick: 첫 인터럽트의 자동 저장 프레임을 캡처 ---------------- */
static volatile uint32_t g_ms;
static volatile uint8_t frame_captured;
static uint32_t saved_frame[8];                    /* R0 R1 R2 R3 R12 LR PC xPSR */

/* naked 트램폴린이 호출(branch)하는 평범한 C 함수. LR(=EXC_RETURN)을 건드리지 않고 branch 했으므로
   이 함수의 표준 프롤로그(push {..,lr})/에필로그(pop {..,pc})가 정상적으로 예외 복귀를 완성한다. */
void systick_report(uint32_t *sp)
{
    g_ms++;
    if (!frame_captured)
    {
        frame_captured = 1;
        for (int i = 0; i < 8; i++) saved_frame[i] = sp[i];
    }
}

__attribute__((naked)) void SysTick_Handler(void)
{
    __asm volatile (
        "mrs r0, msp        \n"   /* r0 = 하드웨어가 방금 만든 스택 프레임의 시작 주소 (이 예제는 항상 MSP만 사용) */
        "b systick_report   \n"); /* call(bl) 이 아니라 branch: LR(EXC_RETURN)을 건드리지 않고 그대로 넘긴다 */
}

/* ---------------- 3) SVC + EXC_RETURN ---------------- */
void SVC_Handler(void)
{
    uint32_t exc_return;
    __asm volatile ("mov %0, lr" : "=r"(exc_return));   /* 이 함수의 첫 동작이어야 한다: 이후 호출(uart_*)이 LR을 덮어쓰기 전에 확보 */

    uart_puts("  SVC_Handler 진입, LR(EXC_RETURN) = "); uart_hex(exc_return);
    uart_puts((exc_return == 0xFFFFFFF9u) ? "  (스레드 모드 + MSP 로 복귀 예정 — 이 예제에서 기대하는 값)\r\n" : "\r\n");
}

int main(void)
{
    uart_init();
    uart_puts("\r\n=== exception model & vector table ===\r\n");

    dump_vector_table();

    /* SysTick 1ms 시작 (HSI 8MHz) */
    SysTick->LOAD = 8000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    while (!frame_captured) {}                     /* 첫 SysTick 인터럽트가 main 을 "끼어들어" 실행되길 대기 */

    uart_puts("\r\n[2] SysTick 첫 진입 시 하드웨어가 자동으로 쌓은 8워드 (PM0056 Table 17)\r\n");
    static const char *const rn[8] = { "R0 ", "R1 ", "R2 ", "R3 ", "R12", "LR ", "PC ", "xPSR" };
    for (int i = 0; i < 8; i++) { uart_puts("  "); uart_puts(rn[i]); uart_puts(" = "); uart_hex(saved_frame[i]); uart_puts("\r\n"); }
    uart_puts("  -> PC 는 인터럽트 되던 순간 main() 이 실행 중이던 명령의 주소 (바로 위 while 루프 어딘가)\r\n");
    uart_puts("     인터럽트가 끝나면 하드웨어가 이 PC 로 그대로 복귀하므로, main 은 아무것도 몰랐던 것처럼 이어서 돈다\r\n");

    uart_puts("\r\n[3] SVC (소프트웨어 인터럽트) 로 EXC_RETURN 직접 확인\r\n");
    uart_puts("  svc 0 실행 전, g_ms="); uart_dec(g_ms); uart_puts("\r\n");
    __asm volatile ("svc 0");                       /* SVC_Handler 로 분기 -> 복귀하면 바로 다음 줄로 이어진다 */
    uart_puts("  svc 0 복귀 후, g_ms="); uart_dec(g_ms); uart_puts("  (SysTick 이 그 사이에도 계속 돌고 있었다는 증거)\r\n");

    uart_puts("\r\n[4] 우선순위는 레지스터의 숫자일 뿐 (SCB->SHPR 계열)\r\n");
    uart_puts("  SysTick 현재 우선순위 = "); uart_dec((uint32_t)NVIC_GetPriority(SysTick_IRQn));
    uart_puts("  (숫자가 작을수록 우선순위가 높다. 자세한 선점 실험은 06_NVIC_Priority_HAL_c 참고)\r\n");

    uart_puts("\r\ndone.\r\n");
    while (1) {}
}

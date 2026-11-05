/**
 * 01_CoreRegisters_Instructions_Reg_c  —  [계층: 레지스터(CMSIS) + 어셈블리]  Cortex-M3 레지스터 파일과 명령어 기초
 *
 * 이 그룹(08_Clock_System)의 출발점. 지금까지의 예제는 "주변장치를 어떻게 쓰는가"를 다뤘다면,
 * 이 예제는 그 아래에서 실제로 명령을 실행하는 **CPU 코어 자체**를 들여다본다 (PM0056 2장, 3장).
 *
 * 1) 레지스터 파일 (PM0056 2.1.3 Core registers, Figure 2)
 *    R0~R12 범용,  R13(SP) 스택 포인터(MSP/PSP 두 벌, 2.1.2 Stacks),  R14(LR) 복귀 주소,  R15(PC) 프로그램 카운터.
 *    특수 레지스터: xPSR(APSR+IPSR+EPSR, PM0056 Figure 3), CONTROL(2.1.1), PRIMASK.
 *    C 코드에서는 이 레지스터가 안 보이지만, CMSIS 함수(`__get_MSP()` 등, cmsis_gcc.h)로 직접 읽을 수 있다.
 *
 * 2) 조건 플래그 N Z C V (PM0056 3.5.3 Condition flags, APSR bit31~28)
 *    ADDS/SUBS 처럼 "S가 붙은" 명령은 연산 결과에 따라 APSR 을 갱신한다. N=음수, Z=0, C=올림/빌림 없음(뺄셈은 반전),
 *    V=부호 있는 오버플로.  일반 ADD(HAL_GPIO_WritePin 같은 C 코드가 컴파일되는 그 명령)는 대개 결과만 쓰고 플래그는 건드리지 않는다.
 *
 * 3) 조건부 실행 IT (PM0056 3.8.7 IT) — Cortex-M0 에는 없는 Thumb-2 특유 기능
 *    "IT-THEN" 블록이 뒤따르는 최대 4개 명령에 조건을 건다. 컴파일러가 짧은 if 문을 분기 없이 IT 로 바꾸기도 한다
 *    (분기 예측 실패 비용을 없애는 최적화). 이 예제는 인라인 어셈블리로 그 원리를 직접 만들어 본다.
 *
 * 4) 스택 동작 (PM0056 3.4.6 LDM/STM, 3.4.7 PUSH/POP)
 *    함수를 호출하면 컴파일러가 자동으로 PUSH(레지스터 저장)/POP(복원)을 넣는다. SP 값이 호출 전/중/후 어떻게
 *    바뀌는지 직접 찍어서 "함수 호출 = 스택에 프레임을 쌓는 것"을 눈으로 확인한다.
 *
 * 5) Thumb 상태 비트 — 함수 포인터의 최하위 비트가 항상 1 인 이유 (분기 시 이 비트로 Thumb 모드임을 표시, ARM 모드는 M3 에 없음)
 *
 * 6) 하드웨어 나눗셈 (PM0056 3.6.3 SDIV/UDIV) — 데이터시트가 강조하는 "Single-cycle multiplication and hardware
 *    division"의 division 쪽. 8비트 MCU 라면 나눗셈이 소프트웨어 루프라 수십~수백 사이클인데, M3 는 명령 하나다.
 *    정확한 사이클 수 측정은 08_Clock_System/10_DWT_Profiling_Reg_c 참고 (여기서는 개념만).
 *
 * ISR: 사용하지 않는다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

/* ---------------- USART2 (레지스터, HSI 8MHz) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);      /* PA2 AF-PP, PA3 플로팅 입력 */
    USART2->BRR = (69u << 4) | 7u;                                 /* 8MHz / 115200 */
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
static void uart_bit(uint32_t v, int bit, const char *name)
{
    uart_puts(name); uart_puts("="); uart_putc((char)('0' + ((v >> bit) & 1u))); uart_puts("  ");
}
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc(b[--i]);
}

/* ---------------- 1) 레지스터 파일 스냅샷 ---------------- */
static void show_registers(void)
{
    uint32_t msp = __get_MSP();
    uint32_t ctrl = __get_CONTROL();
    uint32_t xpsr = __get_xPSR();
    uint32_t primask = __get_PRIMASK();

    uart_puts("\r\n[1] core registers\r\n");
    uart_puts("  MSP (R13, main stack pointer) = "); uart_hex(msp); uart_puts("\r\n");
    uart_puts("  CONTROL: "); uart_bit(ctrl, 1, "SPSEL"); uart_puts("(0=MSP 사용 중, RTOS 없으면 항상 0)  ");
    uart_bit(ctrl, 0, "nPRIV"); uart_puts("(0=특권 스레드 모드)\r\n");
    uart_puts("  xPSR = "); uart_hex(xpsr); uart_puts("  ->  ");
    uart_bit(xpsr, 24, "T"); uart_puts("(Thumb 상태, 항상 1이어야 정상)  ISR_NUMBER=");
    uart_dec(xpsr & 0x1FFu);
    uart_puts(" (0 = 지금 인터럽트 밖, 스레드 모드)\r\n");
    uart_puts("  PRIMASK = "); uart_hex(primask); uart_puts(" (0 = 인터럽트 허용 중)\r\n");
}

/* ---------------- 2) 조건 플래그 N Z C V (인라인 asm 으로 APSR 직접 관찰) ---------------- */
static void show_flags(const char *title, uint32_t a, uint32_t b, int is_add)
{
    uint32_t result, apsr;
    if (is_add)
        __asm volatile ("adds %0, %1, %2" : "=r"(result) : "r"(a), "r"(b) : "cc");
    else
        __asm volatile ("subs %0, %1, %2" : "=r"(result) : "r"(a), "r"(b) : "cc");
    apsr = __get_APSR();

    uart_puts("  "); uart_puts(title); uart_puts(": ");
    uart_hex(a); uart_puts(is_add ? " + " : " - "); uart_hex(b); uart_puts(" = "); uart_hex(result); uart_puts("\r\n    ");
    uart_bit(apsr, 31, "N"); uart_bit(apsr, 30, "Z"); uart_bit(apsr, 29, "C"); uart_bit(apsr, 28, "V"); uart_puts("\r\n");
}

/* ---------------- 3) 조건부 실행 IT (Thumb-2 특유) ---------------- */
static uint32_t classify_it(int32_t x)   /* IT 블록으로 직접 구현한 "x>0 ? 1 : 0" */
{
    uint32_t y;
    __asm volatile (
        "cmp %1, #0      \n"    /* 비교: 플래그만 갱신 (PM0056 3.4.3 Condition flags) */
        "ite gt          \n"    /* If-Then-Else: 다음 두 명령에 조건을 건다 (3.8.7 IT) */
        "movgt %0, #1    \n"    /*   조건(gt) 참이면 실행 */
        "movle %0, #0    \n"    /*   조건(le, else) 이면 실행 */
        : "=r"(y) : "r"(x) : "cc");
    return y;
}

/* ---------------- 4) 스택(PUSH/POP) 동작 관찰 ---------------- */
static uint32_t __attribute__((noinline)) inner_frame(uint32_t depth)
{
    volatile uint32_t local_array[8];                     /* 지역 변수 = 스택에 자리를 만듦 */
    for (int i = 0; i < 8; i++) local_array[i] = depth + (uint32_t)i;
    uint32_t sp_inside = __get_MSP();
    uart_puts("    depth "); uart_dec(depth);
    uart_puts(": SP(진입 후, PUSH 로 레지스터+지역변수 확보) = "); uart_hex(sp_inside);
    uart_puts("  local_array[7] = "); uart_dec(local_array[7]); uart_puts("  (배열이 실제로 이 스택 프레임에 쓰였다는 증거)\r\n");
    if (depth > 0) inner_frame(depth - 1);                 /* 재귀 호출마다 스택이 한 단(프레임)씩 더 쌓인다 */
    return sp_inside;
}

/* ---------------- 6) 하드웨어 나눗셈 (SDIV) ---------------- */
static int32_t hw_divide(int32_t a, int32_t b)
{
    int32_t q;
    __asm volatile ("sdiv %0, %1, %2" : "=r"(q) : "r"(a), "r"(b));   /* PM0056 3.6.3 : 명령 1개, 2~12 사이클 */
    return q;
}

int main(void)
{
    uart_init();
    uart_puts("\r\n=== Cortex-M3 core registers & instructions ===\r\n");

    show_registers();

    uart_puts("\r\n[2] condition flags (N Z C V)\r\n");
    show_flags("overflow (0x7FFFFFFF + 1, 부호있는 오버플로)", 0x7FFFFFFFu, 1u, 1);
    show_flags("zero (5 - 5)", 5u, 5u, 0);
    show_flags("borrow (0 - 1, unsigned underflow -> C=0)", 0u, 1u, 0);

    uart_puts("\r\n[3] conditional execution (IT block)\r\n");
    int32_t samples[3] = { 5, 0, -3 };
    for (int i = 0; i < 3; i++)
    {
        uart_puts("  classify_it("); uart_putc((char)(samples[i] < 0 ? '-' : ' '));
        uart_dec((uint32_t)(samples[i] < 0 ? -samples[i] : samples[i]));
        uart_puts(") = "); uart_dec(classify_it(samples[i])); uart_puts("\r\n");
    }

    uart_puts("\r\n[4] stack (PUSH/POP) while recursing 3 levels deep\r\n");
    uint32_t sp_before = __get_MSP();
    uart_puts("  SP before call = "); uart_hex(sp_before); uart_puts("\r\n");
    inner_frame(3);
    uint32_t sp_after = __get_MSP();
    uart_puts("  SP after  call = "); uart_hex(sp_after);
    uart_puts(sp_after == sp_before ? "  (호출 전과 동일 -> POP 으로 완전히 복원됨)\r\n" : "  !! 복원되지 않음 !!\r\n");

    uart_puts("\r\n[5] Thumb state bit (function pointer LSB)\r\n");
    uint32_t fn_addr = (uint32_t)&main;
    uart_puts("  &main = "); uart_hex(fn_addr); uart_puts("  LSB="); uart_putc((char)('0' + (fn_addr & 1u)));
    uart_puts("  (M3 는 Thumb 전용이라 함수 주소 LSB 가 항상 1: BX/BLX 가 이 비트로 Thumb 진입을 판별)\r\n");

    uart_puts("\r\n[6] hardware divide (SDIV, single instruction)\r\n");
    uart_puts("  1000000 / 7 = "); uart_hex((uint32_t)hw_divide(1000000, 7)); uart_puts(" (정확한 사이클 수는 10_DWT_Profiling 참고)\r\n");

    uart_puts("\r\ndone.\r\n");
    while (1) {}
}

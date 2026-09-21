/**
 * 05_HardFault_Diagnosis_Reg_c  —  [계층: 레지스터(CMSIS) + 어셈블리]  폴트 원인 진단
 *
 * 일부러 폴트를 일으키고, 폴트 핸들러가 "어디서(PC) 왜(CFSR)" 죽었는지 UART 로 출력한다.
 * (HAL 미사용, USART2 를 레지스터로 직접 구동. 클럭은 리셋 기본값 HSI 8MHz)
 *
 * 사용법: 터미널(115200 8N1)에서 숫자 키 입력
 *   1 : 0 으로 나누기         -> UsageFault, UFSR.DIVBYZERO   (CCR.DIV_0_TRP 를 켰기 때문)
 *   2 : 정렬 안 된 주소 접근   -> UsageFault, UFSR.UNALIGNED   (CCR.UNALIGN_TRP 를 켰기 때문)
 *   3 : 존재하지 않는 주소 읽기 -> BusFault,   BFSR.PRECISERR + BFAR(폴트 주소)
 *   4 : 짝수 주소로 분기(Thumb 비트 0) -> UsageFault, UFSR.INVSTATE
 *   폴트가 나면 정보를 출력하고 LD2 를 빠르게 깜빡이며 정지한다. 리셋 버튼으로 다시 시작.
 *
 * 폴트 상세 처리에 쓰는 레지스터 (PM0056 4.4.10 SCB_CFSR, 4.4.11 SCB_HFSR, 2.4.3)
 *   SCB->SHCSR : USGFAULTENA(18), BUSFAULTENA(17), MEMFAULTENA(16)  -> 세부 폴트 핸들러 활성화
 *                (켜지 않으면 모든 폴트가 HardFault 로 "격상"되어 원인 구분이 어렵다)
 *   SCB->CCR   : DIV_0_TRP(4), UNALIGN_TRP(3)
 *   SCB->CFSR  : [7:0] MMFSR, [15:8] BFSR, [31:16] UFSR   (원인 비트, 1 을 써서 지움)
 *   SCB->HFSR  : FORCED(30) 세부 폴트가 HardFault 로 격상됨, VECTTBL(1)
 *   SCB->BFAR / MMFAR : 폴트를 일으킨 주소 (BFSR.BFARVALID / MMFSR.MMARVALID 가 1 일 때만 유효)
 *
 * 스택 프레임 (예외 진입 시 하드웨어가 자동으로 push): R0, R1, R2, R3, R12, LR, PC, xPSR
 *   -> stacked PC 가 "폴트를 일으킨 명령어 주소". 맵 파일/디스어셈블리(arm-none-eabi-addr2line)로 소스 라인 확인.
 *   naked 핸들러가 LR 의 bit2(EXC_RETURN)를 보고 MSP/PSP 중 어느 스택에 프레임이 있는지 판별해 C 함수로 넘긴다.
 *
 * ISR: HardFault/MemManage/BusFault/UsageFault_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

/* ---------------- USART2 레지스터 구동 (PA2 TX, PA3 RX, 8MHz, 115200) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;                    /* GPIOA 클럭 */
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;                  /* USART2 클럭 */

    /* PA2 = AF 푸시풀 50MHz : CRL[11:8]  = CNF(10) MODE(11) = 0xB
       PA3 = 플로팅 입력    : CRL[15:12] = CNF(01) MODE(00) = 0x4 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);

    USART2->BRR = (69u << 4) | 7u;                         /* 8MHz/115200 = 69.44 -> 정수부 69, 분수부 7/16 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) {}                /* SR.TXE : 송신 데이터 레지스터 비었음 */
    USART2->DR = (uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_hex(const char *label, uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts(label);
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(hx[(v >> i) & 0xF]);
    uart_puts("\r\n");
}

/* ---------------- 폴트 리포트 ---------------- */
static void led_panic(void)
{
    /* PA5 = LD2. 진단 출력 후 빠르게 깜빡이며 정지 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);
    while (1)
    {
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < 100000u; i++) {}
    }
}

/* 스택 프레임 포인터를 받아 진단 정보 출력. 핸들러(naked)에서 분기해 온다 */
__attribute__((used)) void fault_report(uint32_t *sp)
{
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;

    uart_puts("\r\n*** FAULT ***\r\n");
    uart_hex("R0   = ", sp[0]);
    uart_hex("R1   = ", sp[1]);
    uart_hex("R2   = ", sp[2]);
    uart_hex("R3   = ", sp[3]);
    uart_hex("R12  = ", sp[4]);
    uart_hex("LR   = ", sp[5]);
    uart_hex("PC   = ", sp[6]);          /* 폴트를 일으킨 명령어 주소 */
    uart_hex("xPSR = ", sp[7]);
    uart_hex("CFSR = ", cfsr);
    uart_hex("HFSR = ", hfsr);

    /* UFSR = CFSR[31:16] */
    if (cfsr & (1u << 25)) uart_puts(" -> UFSR.DIVBYZERO : 0 으로 나눔\r\n");
    if (cfsr & (1u << 24)) uart_puts(" -> UFSR.UNALIGNED : 정렬되지 않은 접근\r\n");
    if (cfsr & (1u << 19)) uart_puts(" -> UFSR.NOCP      : 없는 코프로세서 사용\r\n");
    if (cfsr & (1u << 18)) uart_puts(" -> UFSR.INVPC     : 잘못된 EXC_RETURN\r\n");
    if (cfsr & (1u << 17)) uart_puts(" -> UFSR.INVSTATE  : ARM 상태 진입 시도(Thumb 비트 0 분기 등)\r\n");
    if (cfsr & (1u << 16)) uart_puts(" -> UFSR.UNDEFINSTR: 정의되지 않은 명령어\r\n");
    /* BFSR = CFSR[15:8] */
    if (cfsr & (1u << 12)) uart_puts(" -> BFSR.STKERR    : 예외 진입 스택 push 중 버스 오류\r\n");
    if (cfsr & (1u << 11)) uart_puts(" -> BFSR.UNSTKERR  : 예외 복귀 스택 pop 중 버스 오류\r\n");
    if (cfsr & (1u << 10)) uart_puts(" -> BFSR.IMPRECISERR: 부정확한 버스 오류(쓰기 버퍼 지연)\r\n");
    if (cfsr & (1u << 9))  uart_puts(" -> BFSR.PRECISERR: 정확한 버스 오류\r\n");
    if (cfsr & (1u << 8))  uart_puts(" -> BFSR.IBUSERR   : 명령어 fetch 버스 오류\r\n");
    if (cfsr & (1u << 15)) uart_hex("    BFAR (폴트 주소) = ", SCB->BFAR);
    /* MMFSR = CFSR[7:0] */
    if (cfsr & (1u << 7))  uart_hex("    MMFAR (폴트 주소) = ", SCB->MMFAR);
    if (hfsr & (1u << 30)) uart_puts(" -> HFSR.FORCED   : 세부 폴트가 HardFault 로 격상됨\r\n");

    SCB->CFSR = cfsr;                    /* 원인 비트 지우기 (1 을 써서 클리어) */
    led_panic();
}

/* 예외 진입 시 LR bit2 : 0 = MSP 사용 중이었음, 1 = PSP 사용 중이었음 (EXC_RETURN) */
#define FAULT_HANDLER_ASM(NAME)                            \
    __attribute__((naked)) void NAME(void)                 \
    {                                                      \
        __asm volatile(                                    \
            "tst lr, #4          \n"                       \
            "ite eq              \n"                       \
            "mrseq r0, msp       \n"                       \
            "mrsne r0, psp       \n"                       \
            "b fault_report      \n");                     \
    }

FAULT_HANDLER_ASM(HardFault_Handler)
FAULT_HANDLER_ASM(MemManage_Handler)
FAULT_HANDLER_ASM(BusFault_Handler)
FAULT_HANDLER_ASM(UsageFault_Handler)

/* ---------------- 폴트 유발 함수들 ---------------- */
static void trigger_div0(void)
{
    volatile int a = 10, b = 0;
    volatile int c = a / b;              /* SDIV: CCR.DIV_0_TRP=1 이면 UsageFault */
    (void)c;
}

static void trigger_unaligned(void)
{
    static uint8_t buf[8] __attribute__((aligned(4)));
    volatile uint32_t *p = (volatile uint32_t *)(buf + 1);   /* 4 의 배수가 아닌 주소 */
    volatile uint32_t v = *p;            /* LDR: CCR.UNALIGN_TRP=1 이면 UsageFault */
    (void)v;
}

static void trigger_bus(void)
{
    volatile uint32_t *p = (volatile uint32_t *)0x30000000u; /* F103RB 메모리 맵에 없는 영역 */
    volatile uint32_t v = *p;            /* BusFault (PRECISERR, BFAR=0x30000000) */
    (void)v;
}

static void trigger_invstate(void)
{
    void (*fp)(void) = (void (*)(void))0x20000000u;          /* LSB=0 : Thumb 아님 -> INVSTATE */
    fp();
}

int main(void)
{
    uart_init();

    /* 세부 폴트 핸들러 활성화: 없으면 전부 HardFault 로 격상 */
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;
    /* 0 나누기 / 정렬 위반을 폴트로 처리 */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk | SCB_CCR_UNALIGN_TRP_Msk;

    uart_puts("\r\n[Fault demo] 1:div0  2:unaligned  3:bus  4:invstate\r\n");

    while (1)
    {
        if (USART2->SR & USART_SR_RXNE)                    /* 수신 폴링 */
        {
            char c = (char)USART2->DR;
            switch (c)
            {
            case '1': uart_puts("div by zero...\r\n");   trigger_div0();       break;
            case '2': uart_puts("unaligned...\r\n");     trigger_unaligned();  break;
            case '3': uart_puts("bus fault...\r\n");     trigger_bus();        break;
            case '4': uart_puts("invalid state...\r\n"); trigger_invstate();   break;
            default: break;
            }
        }
    }
}

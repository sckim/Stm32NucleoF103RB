/**
 * 08_DWT_Profiling_Reg_c  —  [계층: 레지스터(CMSIS)]  DWT 사이클 카운터로 코드 실행 시간을 사이클 단위로 측정
 *
 * DWT(Data Watchpoint and Trace) 의 CYCCNT 는 코어 클럭마다 1 씩 증가하는 32비트 카운터다 (Cortex-M3/M4/M7. M0 에는 없음).
 * 타이머 자원 없이 함수 하나의 비용을 정확히 잴 수 있어 최적화 전후 비교에 유용하다.
 *
 * 활성화 (PM0056 / ARM DDI0337)
 *   CoreDebug->DEMCR.TRCENA(bit24) = 1   : DWT/ITM 블록 클럭 허용
 *   DWT->CYCCNT = 0,  DWT->CTRL.CYCCNTENA(bit0) = 1
 *   32비트라서 8MHz 에서 약 536초, 64MHz 에서 약 67초 후 래핑 -> "끝 - 시작"을 부호 없는 뺄셈으로 계산하면 1회 래핑에 안전
 *
 * 이 예제가 측정하는 것 (클럭 = 리셋 기본 HSI 8MHz, Flash 0 대기, 컴파일 -Os)
 *   baseline : 측정 오버헤드(빈 함수 호출)  -> 이후 값은 baseline 을 뺀 순수 비용
 *   add / mul32 / div32 : 정수 덧셈 / 곱셈(MUL 1사이클급) / 나눗셈(SDIV 2~12사이클)
 *   div64 : 64비트 나눗셈 (하드웨어 없음 -> 라이브러리 루틴, 수십~수백 사이클)
 *   float mul / float div : 소프트웨어 부동소수점 (FPU 없음 -> 라이브러리, 수십~수백 사이클)
 *   memcpy 256B : 메모리 복사
 *   GPIO toggle : ODR 읽기-수정-쓰기 1회
 *   결과는 UART(115200)로 "사이클 수 / 8MHz 기준 시간" 표로 출력한다.
 *   * volatile 로 컴파일러의 상수 접기/제거를 막는다. 값은 최적화 수준과 Flash 대기에 따라 달라진다.
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "stm32f1xx.h"
#include <stdint.h>
#include <string.h>

/* ---------------- USART2 레지스터 구동 (PA2 TX, 8MHz, 115200) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);      /* PA2 AF-PP 50MHz, PA3 플로팅 입력 */
    USART2->BRR = (69u << 4) | 7u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_dec(uint32_t v, int width)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    for (int p = i; p < width; p++) uart_putc(' ');
    while (i) uart_putc(b[--i]);
}

/* ---------------- 측정 대상 ---------------- */
static volatile uint32_t va = 123456789u, vb = 7u, vc;
static volatile uint64_t v64a = 0x123456789ABCDEFull, v64b = 12345u, v64c;
static volatile float vf1 = 3.14159f, vf2 = 2.71828f, vf3;
static uint8_t src[256], dst[256];

#define NOINL __attribute__((noinline))
NOINL static void op_empty(void)  { }
NOINL static void op_add(void)    { vc = va + vb; }
NOINL static void op_mul32(void)  { vc = va * vb; }
NOINL static void op_div32(void)  { vc = va / vb; }
NOINL static void op_div64(void)  { v64c = v64a / v64b; }
NOINL static void op_fmul(void)   { vf3 = vf1 * vf2; }
NOINL static void op_fdiv(void)   { vf3 = vf1 / vf2; }
NOINL static void op_memcpy(void) { memcpy(dst, src, sizeof dst); }
NOINL static void op_gpio(void)   { GPIOA->ODR ^= (1u << 5); }

/* f 를 한 번 실행한 사이클 수 (DWT->CYCCNT 차) */
static uint32_t bench(void (*f)(void))
{
    uint32_t best = 0xFFFFFFFFu;
    for (int i = 0; i < 8; i++)                       /* 8회 중 최솟값: 인터럽트/캐시 효과 제거 */
    {
        uint32_t t0 = DWT->CYCCNT;
        f();
        uint32_t d = DWT->CYCCNT - t0;
        if (d < best) best = d;
    }
    return best;
}

static void report(const char *name, void (*f)(void), uint32_t baseline)
{
    uint32_t c = bench(f);
    uint32_t net = (c > baseline) ? c - baseline : 0;
    uart_puts(name);
    for (size_t k = strlen(name); k < 22; k++) uart_putc(' ');
    uart_dec(net, 6);  uart_puts(" cycles   ");
    uart_dec(net * 125u, 7); uart_puts(" ns @8MHz\r\n");   /* 1사이클 = 125ns */
}

int main(void)
{
    uart_init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);      /* PA5 출력 (GPIO 토글 측정용) */
    for (int i = 0; i < 256; i++) src[i] = (uint8_t)i;

    /* DWT 사이클 카운터 활성화 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uart_puts("\r\n[DWT profiling] operation                cycles      time\r\n");
    uint32_t base = bench(op_empty);
    uart_puts("baseline (call+read)  "); uart_dec(base, 6); uart_puts(" cycles\r\n");

    report("uint32 add",            op_add,    base);
    report("uint32 mul",            op_mul32,  base);
    report("uint32 div (SDIV/UDIV)",op_div32,  base);
    report("uint64 div (libgcc)",   op_div64,  base);
    report("float mul (soft)",      op_fmul,   base);
    report("float div (soft)",      op_fdiv,   base);
    report("memcpy 256 B",          op_memcpy, base);
    report("GPIO ODR toggle",       op_gpio,   base);

    uart_puts("done\r\n");
    while (1) {}
}

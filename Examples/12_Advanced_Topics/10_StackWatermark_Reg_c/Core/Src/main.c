/**
 * 10_StackWatermark_Reg_c  —  [계층: 레지스터(CMSIS) + 링커 심볼]  스택 사용량 측정: 페인팅(stack painting)과 하이 워터마크
 *
 * 임베디드에서 가장 잡기 어려운 버그 중 하나가 **스택 오버플로**다. F103 은 MPU 가 없어(Cortex-M3 이지만 미구현) 스택이 힙/데이터를
 * 조용히 덮어써도 폴트가 나지 않는다. 그래서 "스택을 얼마나 썼는지"를 직접 재는 습관이 필요하다.
 *
 * 방법: 스택 페인팅
 *   1) 부팅 직후 사용되지 않은 스택 영역 전체를 **알려진 패턴(0xDEADBEEF)** 으로 채운다.
 *   2) 프로그램이 돌면서 스택이 아래로 자라면 그 영역의 패턴이 지워진다.
 *   3) 나중에 낮은 주소부터 패턴이 남아 있는 곳까지 세면 = **한 번도 안 쓴 여유 공간**.  최대 사용량(하이 워터마크) = 전체 - 여유.
 *
 * 메모리 배치 (링커 스크립트 STM32F103RBHX_FLASH.ld 의 심볼 사용)
 *   |  .data  |  .bss  |  힙(위로 자람)   ....   스택(아래로 자람)  | <- _estack (RAM 끝 = 0x20005000, 초기 MSP)
 *            end ---->                            <---- SP
 *   end = .bss 끝(힙 시작, PROVIDE(end = .)),  _estack = RAM 끝,  _Min_Stack_Size = 링커가 예약한 최소 스택(0x400 = 1KB)
 *   이 예제는 end 에서 현재 SP 근처까지를 페인트한다 (힙은 쓰지 않는다: printf/malloc 을 쓰지 않으므로).
 *
 * 동작 (UART 115200, HSI 8MHz, 레지스터 구동)
 *   - 부팅 시 스택 페인팅, 가용 스택 크기 출력 (링커 예약 최소값과 비교)
 *   - B1(PC13)을 누를 때마다 재귀 깊이를 늘려(각 프레임 약 100바이트) 스택을 더 깊게 쓴다. 깊이 증가 후 워터마크와 여유를 출력.
 *   - 여유가 128바이트 미만이 되면 경고를 출력하고 깊이를 더 늘리지 않는다 (실제 오버플로는 일으키지 않음).
 *
 * 실전 적용: 워터마크 측정을 주기적 로그/디버거 조사에 넣고, 최악 경로(중첩 인터럽트 + 가장 깊은 호출)를 시험한 뒤 스택 크기에
 *            20~50% 여유를 준다. (FreeRTOS 의 uxTaskGetStackHighWaterMark 도 같은 원리이다)
 *
 * ISR: 사용하지 않는다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

extern uint32_t end;                     /* 링커 심볼: .bss 끝 = 힙 시작 */
extern uint32_t _estack;                 /* 링커 심볼: RAM 끝 = 초기 스택 최상단 */

#define PAINT 0xDEADBEEFu
#define GUARD 128u                       /* 이보다 여유가 적으면 더 깊이 들어가지 않는다 */

/* ---------------- USART2 (레지스터) ---------------- */
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
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc(b[--i]);
}

/* ---------------- 스택 페인팅 ---------------- */
static uint32_t stack_bottom(void) { return (uint32_t)&end; }           /* 페인트 시작(낮은 주소) */
static uint32_t stack_top(void)    { return (uint32_t)&_estack; }        /* 스택 최상단(높은 주소) */

static void stack_paint(void)
{
    uint32_t *p = &end;
    uint32_t sp = __get_MSP();
    uint32_t *stop = (uint32_t *)(sp - 64u);                            /* 지금 사용 중인 프레임(과 여유 64바이트)은 건드리지 않음 */
    while (p < stop) *p++ = PAINT;
}

/* 낮은 주소부터 패턴이 연속으로 남아 있는 바이트 수 = 한 번도 쓰지 않은 여유 */
static uint32_t stack_free_bytes(void)
{
    uint32_t *p = &end;
    uint32_t n = 0;
    while (p < (uint32_t *)__get_MSP() && *p == PAINT) { p++; n += 4; }
    return n;
}

static uint32_t stack_used_max(void)                                    /* 하이 워터마크: 지금까지 최대 사용량 */
{
    return stack_top() - (stack_bottom() + stack_free_bytes());
}

/* 재귀: 깊이 n 이면 프레임 n 개를 쌓는다. 지역 배열 + volatile 로 컴파일러가 프레임을 없애지 못하게 한다 */
static uint32_t __attribute__((noinline)) recurse(uint32_t n)
{
    volatile uint32_t pad[24];                                          /* 약 96바이트 + 저장 레지스터 */
    for (uint32_t i = 0; i < 24; i++) pad[i] = n + i;
    if (n == 0) return pad[0] + pad[23];
    return recurse(n - 1) + pad[n % 24];
}

int main(void)
{
    stack_paint();                                                      /* 가장 먼저: 아직 스택을 거의 안 썼을 때 */

    uart_init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);         /* PC13 입력(풀업) */
    GPIOC->BSRR = (1u << 13);

    uart_puts("\r\n[stack watermark] region ");
    uart_dec(stack_top() - stack_bottom()); uart_puts(" B painted (end..._estack), linker min stack = 1024 B\r\n");

    uint32_t depth = 0;
    uart_puts("press B1 to recurse deeper\r\n");
    while (1)
    {
        if ((GPIOC->IDR & (1u << 13)) == 0)                            /* B1 눌림 */
        {
            for (volatile uint32_t d = 0; d < 400000u; d++) {}          /* 채터링 방지 */
            while ((GPIOC->IDR & (1u << 13)) == 0) {}
            for (volatile uint32_t d = 0; d < 400000u; d++) {}

            uint32_t free_now = stack_free_bytes();
            if (free_now < GUARD + 12u * 128u)                          /* 다음 단계(12프레임 추가, 약 1.2KB)가 위험하면 중단 */
            {
                uart_puts("!! stack nearly exhausted: stop deepening (free "); uart_dec(free_now); uart_puts(" B)\r\n");
                continue;
            }
            depth += 12;
            uint32_t r = recurse(depth);
            (void)r;

            uart_puts("depth=");        uart_dec(depth);
            uart_puts("  max stack used="); uart_dec(stack_used_max());
            uart_puts(" B  free="); uart_dec(stack_free_bytes()); uart_puts(" B\r\n");
        }
    }
}

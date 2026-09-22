/**
 * 12_IAP_App_Reg_c  —  [계층: 레지스터(CMSIS)]  IAP 응용 프로그램: 플래시 0x08008000 에서 시작하는 앱 (11_IAP_Bootloader 가 점프해 들어옴)
 *
 * 부트로더 뒤 영역에서 동작하려면 세 가지가 필요하다
 *   1) 링크 위치: ldscript.ld 의 FLASH ORIGIN = 0x08008000, LENGTH = 96K  -> 벡터 테이블과 코드가 이 주소부터 배치된다
 *   2) 벡터 테이블 오프셋: SCB->VTOR = 0x08008000  (부트로더가 점프 전에 설정하지만, 앱을 단독 실행/디버거 실행할 때를 위해 앱도 시작할 때 설정)
 *   3) 인터럽트 허용: 부트로더가 __disable_irq() 한 상태로 넘어오므로 앱이 __enable_irq() 해야 한다
 *   (부트로더 없이 이 앱을 0x08008000 에 직접 올려도 리셋 벡터가 0x08000000 을 가리키므로 자동 실행되지 않는다: 반드시 부트로더가 먼저 필요)
 *
 * 동작: UART(115200)로 "Application running" + VTOR/현재 SP 를 출력하고, SysTick 인터럽트(1ms)를 이용해 LD2 를 200ms 주기로 점멸.
 *       SysTick_Handler 가 정상 동작해 점멸이 보이면 "벡터 테이블 재배치가 제대로 됐다"는 증거이다
 *       (VTOR 를 안 바꾸면 SysTick 이 부트로더 영역의 핸들러로 가서 멈추거나 오동작한다).
 *
 * ISR: SysTick_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

#define APP_ADDR 0x08008000UL

static volatile uint32_t g_ms;
void SysTick_Handler(void) { g_ms++; }

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

int main(void)
{
    SCB->VTOR = APP_ADDR;                                        /* 2) 벡터 테이블을 이 앱의 것으로 */
    __enable_irq();                                              /* 3) 부트로더가 꺼 둔 인터럽트 허용 */

    uart_init();
    uart_puts("\r\n[application] running. VTOR="); uart_hex(SCB->VTOR);
    uart_puts(" SP="); uart_hex(__get_MSP()); uart_puts("\r\n");

    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);    /* LD2 */
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    uint32_t t_led = 0, beats = 0;
    while (1)
    {
        if ((uint32_t)(g_ms - t_led) >= 200u)
        {
            t_led += 200u;
            GPIOA->ODR ^= (1u << 5);
            if ((++beats % 10u) == 0u) { uart_puts("app alive, uptime ms="); uart_hex(g_ms); uart_puts("\r\n"); }
        }
    }
}

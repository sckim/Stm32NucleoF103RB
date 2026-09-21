/**
 * 10_DeviceInfo_UniqueID_Reg_c  —  [계층: 레지스터(CMSIS)]  칩 식별 정보: 96비트 고유 ID, 플래시 크기, 디바이스 ID, 리셋 원인
 *
 * 읽는 정보와 위치 (RM0008 / DS5319 데이터시트 "Device electronic signature")
 *   고유 ID (96비트)      : 주소 0x1FFFF7E8 ~ 0x1FFFF7F3 (읽기 전용, 공장에서 기록). 칩마다 다름 -> 시리얼 번호/암호화 키 시드/통신 주소 생성
 *   플래시 크기 (KB)       : 0x1FFFF7E0 (16비트). Nucleo-F103RB 는 128
 *   DBGMCU->IDCODE (0xE0042000) : DEV_ID[11:0] = 0x410 (medium-density), REV_ID[31:16] (실리콘 리비전)
 *   SCB->CPUID (0xE000ED00)     : 코어 종류/리비전. Cortex-M3 r1p1 이면 0x411FC231
 *   RCC->CSR (0x40021024)       : 리셋 원인 플래그
 *        LPWRRSTF(31) 저전력 관리 리셋, WWDGRSTF(30), IWDGRSTF(29), SFTRSTF(28) 소프트웨어(NVIC_SystemReset),
 *        PORRSTF(27) 전원 인가/강하, PINRSTF(26) NRST 핀,  RMVF(24) 에 1 을 쓰면 플래그 삭제
 *   메모리: 이 소자는 Flash 128KB / SRAM 20KB. 링커 스크립트가 아니라 실제 소자 기록값을 읽어 확인한다.
 *
 * 동작: 부팅 때 위 정보를 UART(115200, HSI 8MHz)로 한 번 출력하고 LD2 를 점멸.
 *       NRST 버튼(PINRST), 전원 재인가(PORRST) 를 바꿔 가며 리셋 원인 표시가 바뀌는 것을 확인.
 *       (다른 예제에서 소프트웨어/워치독 리셋을 만들고 이 예제의 리셋 원인 표시 코드를 가져다 쓰면 원인 진단에 유용)
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

#define UID_BASE_ADDR   0x1FFFF7E8UL
#define FLASH_SIZE_ADDR 0x1FFFF7E0UL

/* ---------------- USART2 레지스터 구동 ---------------- */
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
static void uart_hex(uint32_t v, int digits)
{
    static const char hx[] = "0123456789ABCDEF";
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) uart_putc(hx[(v >> i) & 0xF]);
}
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc(b[--i]);
}

int main(void)
{
    /* 리셋 원인은 다른 초기화 전에 읽고 곧바로 지운다 (RCC->CSR.RMVF) */
    uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;

    uart_init();

    uart_puts("\r\n=== Device info ===\r\n");

    uart_puts("Core CPUID    : 0x"); uart_hex(SCB->CPUID, 8);
    uart_puts(((SCB->CPUID >> 4) & 0xFFF) == 0xC23 ? "  (Cortex-M3)\r\n" : "  (unexpected core)\r\n");

    uint32_t idc = DBGMCU->IDCODE;
    uart_puts("DEV_ID        : 0x"); uart_hex(idc & 0xFFF, 3);
    uart_puts((idc & 0xFFF) == 0x410 ? "  (STM32F10x medium-density)\r\n" : "\r\n");
    uart_puts("REV_ID        : 0x"); uart_hex(idc >> 16, 4); uart_puts("\r\n");

    uart_puts("Flash size    : "); uart_dec(*(volatile uint16_t *)FLASH_SIZE_ADDR); uart_puts(" KB\r\n");

    /* 96비트 고유 ID: 32비트 워드 3개 (F1 은 바이트 순서 그대로 읽으면 됨) */
    uart_puts("Unique ID     : ");
    for (int w = 2; w >= 0; w--)
    {
        uart_hex(*(volatile uint32_t *)(UID_BASE_ADDR + 4u * (uint32_t)w), 8);
        if (w) uart_putc('-');
    }
    uart_puts("\r\n");

    uart_puts("Reset cause   :");
    if (csr & RCC_CSR_LPWRRSTF) uart_puts(" LOW-POWER");
    if (csr & RCC_CSR_WWDGRSTF) uart_puts(" WWDG");
    if (csr & RCC_CSR_IWDGRSTF) uart_puts(" IWDG");
    if (csr & RCC_CSR_SFTRSTF)  uart_puts(" SOFTWARE");
    if (csr & RCC_CSR_PORRSTF)  uart_puts(" POWER-ON");
    if (csr & RCC_CSR_PINRSTF)  uart_puts(" NRST-PIN");
    uart_puts("  (CSR=0x"); uart_hex(csr, 8); uart_puts(")\r\n");

    /* LD2 점멸 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);
    while (1)
    {
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < 300000u; i++) {}
    }
}

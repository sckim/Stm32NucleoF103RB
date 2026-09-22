/**
 * 02_MemoryMap_Reg_c  —  [계층: 레지스터(CMSIS)]  4GB 주소 공간과 메모리 매핑 I/O: "레지스터"는 사실 그냥 메모리 주소다
 *
 * 지금까지 `GPIOA->ODR = ...` 처럼 화살표로 레지스터를 썼다. 이 예제는 그 뒤에 숨은 사실을 보여준다:
 * **GPIOA 는 특별한 것이 아니라 0x40010800 이라는 "그냥 메모리 주소"에 놓인 구조체 포인터**이고,
 * 코어(Cortex-M3)는 SRAM 을 읽는 것과 똑같은 명령(LDR/STR)으로 그 주소를 읽고 쓴다. 이것이 memory-mapped I/O 다.
 *
 * Cortex-M3 는 4GB(32비트) 주소 공간을 고정된 용도로 미리 나눠 둔다 (PM0056 2.2.1 Memory regions, Figure 9 / RM0008 3.3 Memory map)
 *   0x00000000 ~ 0x1FFFFFFF  Code      : 부팅 시 이 영역이 Flash 또는 시스템 메모리(내장 부트로더)로 별칭(alias) 된다 (부트 핀으로 선택, AN2606 참고)
 *   0x08000000 ~ 0x0801FFFF  (Code 안) : 이 소자의 실제 Flash 128KB
 *   0x20000000 ~ 0x2FFFFFFF  SRAM      : 이 소자는 20KB만 존재(0x20000000~0x20004FFF), 나머지는 예약
 *   0x40000000 ~ 0x5FFFFFFF  Peripheral: APB1(저속)/APB2(고속)/AHB 버스에 달린 모든 주변장치 레지스터
 *   0x60000000 ~ 0x9FFFFFFF  FSMC 외부 메모리 뱅크 (F103RB 의 FSMC 는 미탑재 — 09_MCO_ClockOut 이전 그룹표 참고)
 *   0xE0000000 ~ 0xE00FFFFF  PPB(Private Peripheral Bus): NVIC, SCB, SysTick, DWT 같은 **코어 자체**의 레지스터
 *
 * 버스 계층 (RM0008 3.1 System architecture): Cortex-M3 코어 -- AHB(72MHz) -- { GPIO/DMA/Flash/... , APB2 브리지(고속) -- {USART1,ADC,TIM1..} , APB1 브리지(저속, 최대36MHz) -- {USART2/3,I2C,TIM2..,PWR} }
 *   그래서 GPIOA(APB2)는 0x4001xxxx, TIM2(APB1)는 0x4000xxxx, RCC(AHB)는 0x40021000 처럼 버스별로 주소 블록이 나뉜다.
 *
 * 이 예제가 보여주는 것 (UART 115200, HSI 8MHz)
 *   1) 위 영역 이름과 범위를 표로 출력
 *   2) CMSIS 가 이미 계산해 둔 포인터(GPIOA, RCC, SCB, NVIC, SysTick)의 실제 주소를 출력해 위 표와 맞는지 확인
 *   3) 같은 GPIOA->ODR 레지스터를 "구조체 화살표"와 "리터럴 주소를 직접 역참조"하는 두 가지 방법으로 건드려 **완전히 같은 것**임을 증명
 *      (세 번째 방법인 비트 밴딩 별칭은 01_GPIO/09_BitBanding_Reg_c 에서 깊게 다룬다)
 *   4) 문자열 리터럴(Flash=Code 영역)과 전역 변수(SRAM 영역), 그리고 현재 스택 포인터(SRAM, 위에서 아래로 자람)의 주소를 출력해
 *      "내 프로그램의 코드/데이터/스택이 실제로 어느 영역에 있는지" 표와 대조
 *   5) 리틀 엔디안(PM0056 2.2.6): 32비트 값을 메모리에 쓰고 바이트 단위로 읽어 낮은 주소에 하위 바이트가 오는 것을 확인
 *
 * ISR: 사용하지 않는다.
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

/* GPIOA 를 "구조체 화살표"가 아니라 "리터럴 주소를 직접 역참조"해서 건드리는 버전.
   0x40010800 = GPIOA 베이스,  +0x0C = ODR 오프셋(RM0008 9.4.7 GPIOx_ODR) */
#define GPIOA_BASE_LITERAL  0x40010800UL
#define GPIOA_ODR_LITERAL   (*(volatile uint32_t *)(GPIOA_BASE_LITERAL + 0x0CUL))

static const char flash_string[] = "이 문자열은 Flash(Code 영역)에 있다";   /* 읽기 전용 데이터 -> .rodata -> Flash */
static uint32_t sram_variable = 0x11223344u;                                /* 전역 변수 -> .data -> SRAM */

int main(void)
{
    uart_init();
    uart_puts("\r\n=== 4GB address space & memory-mapped I/O ===\r\n");

    uart_puts("\r\n[1] memory regions (PM0056 Fig.9 / RM0008 3.3)\r\n");
    uart_puts("  Code       : "); uart_hex(0x00000000u); uart_puts(" ~ "); uart_hex(0x1FFFFFFFu); uart_puts("  (Flash 0x08000000~0x0801FFFF 포함)\r\n");
    uart_puts("  SRAM       : "); uart_hex(0x20000000u); uart_puts(" ~ "); uart_hex(0x3FFFFFFFu); uart_puts("  (실제 20KB만 존재: ~0x20004FFF)\r\n");
    uart_puts("  Peripheral : "); uart_hex(0x40000000u); uart_puts(" ~ "); uart_hex(0x5FFFFFFFu); uart_puts("  (APB1/APB2/AHB 주변장치)\r\n");
    uart_puts("  FSMC bank  : "); uart_hex(0x60000000u); uart_puts(" ~ "); uart_hex(0x9FFFFFFFu); uart_puts("  (F103RB 는 FSMC 미탑재)\r\n");
    uart_puts("  PPB(코어)  : "); uart_hex(0xE0000000u); uart_puts(" ~ "); uart_hex(0xE00FFFFFu); uart_puts("  (NVIC/SCB/SysTick/DWT)\r\n");

    uart_puts("\r\n[2] CMSIS 가 계산해 둔 포인터 == 위 표의 주소인지 확인\r\n");
    uart_puts("  GPIOA  = "); uart_hex((uint32_t)GPIOA); uart_puts("  (APB2, 예상 0x40010800)\r\n");
    uart_puts("  TIM2   = "); uart_hex((uint32_t)TIM2);  uart_puts("  (APB1, 예상 0x40000000 — 저속 버스라 낮은 오프셋)\r\n");
    uart_puts("  RCC    = "); uart_hex((uint32_t)RCC);   uart_puts("  (AHB,  예상 0x40021000)\r\n");
    uart_puts("  NVIC   = "); uart_hex((uint32_t)NVIC);  uart_puts("  (PPB,  예상 0xE000E100)\r\n");
    uart_puts("  SysTick= "); uart_hex((uint32_t)SysTick); uart_puts("  (PPB,  예상 0xE000E010)\r\n");

    uart_puts("\r\n[3] 같은 레지스터, 두 가지 접근 방식이 정말 같은지 증명\r\n");
    GPIOA->ODR |= (1u << 5);                                       /* (a) 구조체 화살표로 LD2 켬 */
    uart_puts("  (a) GPIOA->ODR |= (1<<5) 로 켠 뒤, (b) 리터럴 주소로 읽으면: ");
    uart_hex(GPIOA_ODR_LITERAL); uart_puts("  (bit5 이 서 있으면 두 접근이 같은 메모리를 가리킨다는 뜻)\r\n");
    GPIOA_ODR_LITERAL &= ~(1u << 5);                                /* (b) 리터럴 주소로 다시 끔 */
    uart_puts("  (b) 리터럴 주소로 끈 뒤, (a) 구조체로 읽으면: "); uart_hex(GPIOA->ODR); uart_puts("  (bit5 이 내려갔어야 함)\r\n");

    uart_puts("\r\n[4] 내 프로그램의 코드/데이터/스택은 실제로 어디에 있는가\r\n");
    uart_puts("  문자열 리터럴 주소 = "); uart_hex((uint32_t)flash_string); uart_puts("  (0x08... 이면 Flash=Code 영역, 표 [1]과 대조)\r\n");
    uart_puts("  전역 변수   주소 = "); uart_hex((uint32_t)&sram_variable); uart_puts("  (0x20... 이면 SRAM 영역)\r\n");
    uart_puts("  현재 SP(MSP) 주소 = "); uart_hex(__get_MSP()); uart_puts("  (역시 0x20... SRAM, 위에서 아래로 자람)\r\n");

    uart_puts("\r\n[5] little-endian (PM0056 2.2.6): 낮은 주소에 하위 바이트\r\n");
    static uint32_t word = 0x01020304u;
    volatile uint8_t *b = (volatile uint8_t *)&word;
    uart_puts("  word = "); uart_hex(word); uart_puts("\r\n  byte[0](가장 낮은 주소) = "); uart_hex(b[0]);
    uart_puts("  <- 0x04 여야 정상 (리틀 엔디안: 최하위 바이트가 최하위 주소)\r\n  byte[3](가장 높은 주소) = "); uart_hex(b[3]);
    uart_puts("  <- 0x01 이면 정상\r\n");

    uart_puts("\r\ndone.\r\n");
    while (1) {}
}

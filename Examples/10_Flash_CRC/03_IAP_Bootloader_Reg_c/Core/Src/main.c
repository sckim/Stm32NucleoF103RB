/**
 * 03_IAP_Bootloader_Reg_c  —  [계층: 레지스터(CMSIS)]  IAP(In-Application Programming) 부트로더: 응용 프로그램으로 점프
 *
 * 짝 예제: 04_IAP_App_Reg_c (응용 프로그램). 두 프로젝트를 각각 빌드해서 플래시의 서로 다른 영역에 올린다.
 *
 * Flash 배치 (F103RB 128KB)
 *   0x08000000 ~ 0x08007FFF (32KB) : 이 부트로더   (ldscript.ld 의 FLASH LENGTH = 32K)
 *   0x08008000 ~ 0x0801FFFF (96KB) : 응용 프로그램 (04_IAP_App_Reg_c, FLASH ORIGIN = 0x08008000)
 *
 * 부트로더가 하는 일
 *   1) 부팅하면 UART 로 배너를 출력한다. B1(PC13)을 누른 채 리셋하면 "부트로더 유지" 모드(LD2 빠른 점멸)로 남는다 (펌웨어 업데이트 대기 자리).
 *   2) 응용 프로그램 유효성 검사: 응용의 벡터 테이블 첫 두 워드를 읽어
 *        [0x08008000] = 초기 MSP  -> SRAM 범위(0x20000000~0x20004FFF, 20KB)여야 함
 *        [0x08008004] = Reset 벡터 -> 응용 Flash 범위 안이고 Thumb 비트(bit0)=1 이어야 함
 *      비어 있는 플래시(0xFFFFFFFF)나 엉뚱한 값이면 점프하지 않는다.
 *   3) 유효하면 점프 준비 후 분기:
 *        - 인터럽트 전부 끄기(__disable_irq), SysTick 정지, NVIC 의 모든 인터럽트 비활성/펜딩 해제  <- 응용이 부트로더의 잔여 인터럽트에 방해받지 않도록
 *        - SCB->VTOR = 0x08008000  : 벡터 테이블을 응용의 것으로 교체 (이후 모든 인터럽트가 응용의 핸들러로 감)
 *        - __set_MSP(초기 MSP)     : 스택 포인터를 응용의 값으로
 *        - 응용의 Reset_Handler 로 함수 호출처럼 분기 (돌아오지 않는다)
 *   4) 유효한 응용이 없으면 "no application" 을 출력하고 LD2 를 느리게 점멸.
 *
 * 하드웨어 대응
 *   SCB->VTOR (PM0056 4.4.4 SCB_VTOR) : 벡터 테이블 오프셋 (최소 정렬: 벡터 개수에 따라 큰 2 의 거듭제곱, F103 은 0x200 배수)
 *   NVIC->ICER[], NVIC->ICPR[] : 인터럽트 비활성 / 펜딩 해제,   SysTick->CTRL = 0
 *
 * 사용법 (PlatformIO)
 *   1) 이 프로젝트를 빌드/업로드 (부트로더 32KB 영역에 기록)
 *   2) 04_IAP_App_Reg_c 를 빌드/업로드. 업로드 도구는 ELF 의 주소(0x08008000~)에만 기록하므로 부트로더는 지워지지 않는다
 *      (OpenOCD 의 "program" 은 필요한 페이지만 지운다. 전체 칩 삭제 옵션은 쓰지 말 것)
 *   3) 리셋하면 부트로더 배너 -> 응용이 실행되어 "Application running" 이 출력된다.
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

#define APP_ADDR      0x08008000UL
#define FLASH_END     0x08020000UL           /* 128KB */
#define SRAM_BASE_A   0x20000000UL
#define SRAM_END      0x20005000UL           /* 20KB */

/* ---------------- USART2 (레지스터) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);
    USART2->BRR = (69u << 4) | 7u;               /* 8MHz / 115200 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_flush(void)  { while (!(USART2->SR & USART_SR_TC)) {} }     /* 마지막 바이트까지 나갔는지 (SR.TC) */
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_hex(uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(hx[(v >> i) & 0xF]);
}

static int app_is_valid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4);
    int sp_ok = (sp >= SRAM_BASE_A && sp <= SRAM_END);
    int pc_ok = ((pc & 1u) == 1u) && (pc > APP_ADDR) && (pc < FLASH_END);     /* Thumb 비트 필수 */
    return sp_ok && pc_ok;
}

static void jump_to_app(void)
{
    uint32_t app_sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t app_pc = *(volatile uint32_t *)(APP_ADDR + 4);

    __disable_irq();
    SysTick->CTRL = 0;                                           /* SysTick 정지 */
    for (int i = 0; i < 2; i++)                                  /* F103 인터럽트 60개 미만 -> ICER[0..1] 로 충분 */
    {
        NVIC->ICER[i] = 0xFFFFFFFFu;                             /* 모든 인터럽트 비활성 */
        NVIC->ICPR[i] = 0xFFFFFFFFu;                             /* 펜딩 해제 */
    }
    SCB->VTOR = APP_ADDR;                                        /* 벡터 테이블 교체 */
    __set_MSP(app_sp);                                           /* 응용의 초기 스택 포인터 */
    ((void (*)(void))app_pc)();                                  /* 응용 Reset_Handler 로 분기 */
    while (1) {}
}

int main(void)
{
    uart_init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);    /* LD2 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);    /* PC13 입력(풀업 선택) */
    GPIOC->BSRR = (1u << 13);

    uart_puts("\r\n[bootloader v1] app @ "); uart_hex(APP_ADDR);
    uart_puts(", MSP="); uart_hex(*(volatile uint32_t *)APP_ADDR);
    uart_puts(", Reset="); uart_hex(*(volatile uint32_t *)(APP_ADDR + 4)); uart_puts("\r\n");

    int stay = ((GPIOC->IDR & (1u << 13)) == 0);                 /* B1 눌림 = 부트로더 유지 */
    if (!stay && app_is_valid())
    {
        uart_puts("valid application -> jumping\r\n");
        uart_flush();                                            /* 메시지가 잘리지 않게 송신 완료까지 대기 */
        jump_to_app();
    }

    uart_puts(stay ? "B1 held: staying in bootloader\r\n" : "no valid application -> staying in bootloader\r\n");
    uint32_t delay = stay ? 100000u : 600000u;                   /* 유지 모드는 빠르게, 앱 없음은 느리게 점멸 */
    while (1)
    {
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < delay; i++) {}
    }
}

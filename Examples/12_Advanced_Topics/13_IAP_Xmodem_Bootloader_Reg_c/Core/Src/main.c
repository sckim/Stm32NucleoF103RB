/**
 * 13_IAP_Xmodem_Bootloader_Reg_c  —  [계층: 레지스터(CMSIS)]  UART 로 새 펌웨어를 받아 Flash 에 기록하는 부트로더 (XMODEM-CRC)
 *
 * 11_IAP_Bootloader 는 "이미 Flash 에 있는 앱으로 점프"만 했다. 이 예제는 **PC 에서 UART 로 .bin 을 보내 앱 영역에 직접 기록**한다.
 * ST-LINK 없이 현장에서 펌웨어를 업데이트하는 IAP(In-Application Programming)의 전형적인 구조이다. 앱은 `12_IAP_App_Reg_c` 를 사용한다.
 *
 * Flash 배치: 0x08000000~0x08007FFF (32KB) 이 부트로더 / 0x08008000~0x0801FFFF (96KB) 앱  (ldscript.ld 가 부트로더를 32KB 로 제한)
 *
 * 사용법
 *   1) 이 프로젝트를 ST-LINK 로 한 번 업로드한다 (부트로더 영역).
 *   2) 앱 프로젝트(12_IAP_App_Reg_c)를 빌드해 `.pio/build/nucleo_f103rb/firmware.bin` 을 만든다.
 *   3) **B1(PC13)을 누른 채 리셋**하거나, 유효한 앱이 없을 때 부트로더는 "XMODEM 수신 대기"로 들어간다 (최대 60초).
 *      터미널 프로그램(Tera Term: File > Transfer > XMODEM > Send, "Checksum" 아닌 **CRC** 옵션; 115200 8N1)로 `firmware.bin` 을 보낸다.
 *   4) 전송이 끝나면 앱을 검사하고 바로 점프한다. 다음 부팅부터는 B1 을 누르지 않으면 곧바로 앱이 실행된다.
 *
 * XMODEM-CRC 프로토콜 (수신 측 동작)
 *   수신자가 'C'(0x43)를 1초마다 보내 CRC 모드 시작을 알린다 -> 송신자가 패킷을 보낸다:
 *     [SOH 0x01 | STX 0x02] [블록 번호] [255-블록번호] [데이터 128 | 1024 바이트] [CRC16 상위] [CRC16 하위]
 *   수신자는 블록 번호 쌍과 CRC16(다항식 0x1021, 초기값 0)을 검사해 맞으면 ACK(0x06), 틀리면 NAK(0x15) -> 송신자가 재전송.
 *   송신자가 EOT(0x04)를 보내면 ACK 하고 종료. 같은 블록이 다시 오면(ACK 유실) 기록하지 않고 ACK 만 한다. CAN(0x18)은 취소.
 *   마지막 블록은 남는 바이트를 0x1A 로 채워 온다 (그대로 Flash 에 기록되어도 무해).
 *
 * Flash 기록 (RM0008 / PM0075, 레지스터 직접 조작)
 *   잠금 해제: FLASH->KEYR 에 0x45670123, 0xCDEF89AB 를 차례로 쓴다.  페이지 소거: CR.PER=1, AR=주소, CR.STRT=1, SR.BSY 대기.
 *   반워드(16비트) 프로그램: CR.PG=1, 16비트 쓰기, BSY 대기.  (소거되지 않은 위치는 프로그램할 수 없다.)
 *   각 1KB 페이지의 첫 쓰기 직전에 그 페이지를 소거한다. 소거/프로그램 중에는 Flash 접근이 정지하므로 이 코드는 RAM 이 아니라 Flash 에서 돌아도
 *   같은 뱅크가 아닌 한 문제없지만(부트로더 영역과 앱 영역은 같은 Flash 이므로 읽기가 잠시 멈출 뿐), 전송 속도(115200bps)가 느려 충분히 여유 있다.
 *
 * 안전 장치: 앱 영역(0x08008000~0x0801FFFF) 밖으로 쓰지 않는다. 전송이 중간에 실패하면 벡터 테이블이 지워진 상태라 앱 검사에서 탈락해 점프하지 않는다.
 *
 * 인터럽트를 사용하지 않는다 (폴링). ms 타이밍은 SysTick 의 COUNTFLAG 를 폴링한다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

#define APP_ADDR    0x08008000UL
#define FLASH_END   0x08020000UL
#define PAGE_SIZE   1024UL
#define SRAM_BEGIN  0x20000000UL
#define SRAM_END    0x20005000UL

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18

/* ---------------- USART2 / SysTick(폴링) ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);
    USART2->BRR = (69u << 4) | 7u;                      /* 8MHz / 115200 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void uart_putc(uint8_t c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = c; }
static void uart_flush(void)     { while (!(USART2->SR & USART_SR_TC)) {} }
static void uart_puts(const char *s) { while (*s) uart_putc((uint8_t)*s++); }
static void uart_hex(uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc((uint8_t)hx[(v >> i) & 0xF]);
}
static void uart_dec(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) uart_putc((uint8_t)b[--i]);
}

static void ms_timer_init(void)                          /* 1ms 마다 COUNTFLAG 가 세트되는 자유 실행 카운터 (인터럽트 없음) */
{
    SysTick->LOAD = 8000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* 최대 timeout_ms 동안 1바이트 대기. 없으면 -1 */
static int uart_getc_timeout(uint32_t timeout_ms)
{
    (void)SysTick->CTRL;                                 /* COUNTFLAG 지우기(읽으면 지워짐) */
    while (timeout_ms)
    {
        if (USART2->SR & USART_SR_RXNE) return (int)(USART2->DR & 0xFF);
        if (USART2->SR & USART_SR_ORE) (void)USART2->DR; /* 오버런 플래그는 DR 읽기로 해제 */
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) timeout_ms--;
    }
    return -1;
}

/* ---------------- Flash (레지스터) ---------------- */
static void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = 0x45670123UL; FLASH->KEYR = 0xCDEF89ABUL; }
}
static void flash_lock(void)  { FLASH->CR |= FLASH_CR_LOCK; }
static void flash_wait(void)  { while (FLASH->SR & FLASH_SR_BSY) {} }

static int flash_erase_page(uint32_t addr)
{
    flash_wait();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;   /* 오류/완료 플래그는 1 을 써서 지운다 */
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;
    flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;
    return !(FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR));
}

static int flash_program_half(uint32_t addr, uint16_t half)
{
    flash_wait();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = half;
    flash_wait();
    FLASH->CR &= ~FLASH_CR_PG;
    return *(volatile uint16_t *)addr == half;                       /* 읽어서 검증 */
}

/* ---------------- XMODEM-CRC ---------------- */
static uint16_t crc16_ccitt(const uint8_t *p, uint32_t n)
{
    uint16_t crc = 0;
    while (n--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++) crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* 반환: 0 = 성공, 음수 = 실패 코드 (-1 시작 시간 초과, -2 재시도 초과, -3 취소, -4 순서/주소 오류, -5 Flash 오류) */
static int xmodem_receive(uint32_t dest, uint32_t max_len, uint32_t *total)
{
    static uint8_t data[1024];
    uint8_t expect = 1;
    uint32_t addr = dest;
    int started = 0, retries = 0, tries = 0;
    *total = 0;

    for (;;)
    {
        int c = uart_getc_timeout(1000);
        if (c < 0)
        {
            if (!started)                                            /* 아직 시작 전: 'C' 로 CRC 모드 요청 (최대 60번) */
            {
                if (++tries > 60) return -1;
                uart_putc('C');
            }
            else
            {
                if (++retries > 10) return -2;
                uart_putc(NAK);
            }
            continue;
        }
        if (c == EOT) { uart_putc(ACK); return 0; }
        if (c == CAN) return -3;
        if (c != SOH && c != STX) continue;                          /* 잡음 무시 */

        uint32_t size = (c == SOH) ? 128u : 1024u;
        int blk = uart_getc_timeout(1000), nblk = uart_getc_timeout(1000);
        int ok = (blk >= 0 && nblk >= 0 && (uint8_t)(blk + nblk) == 0xFF);
        for (uint32_t i = 0; ok && i < size; i++)
        {
            int d = uart_getc_timeout(1000);
            if (d < 0) ok = 0; else data[i] = (uint8_t)d;
        }
        int ch = ok ? uart_getc_timeout(1000) : -1, cl = ok ? uart_getc_timeout(1000) : -1;
        if (ok && (ch < 0 || cl < 0 || crc16_ccitt(data, size) != (uint16_t)((ch << 8) | cl))) ok = 0;

        if (!ok)                                                     /* 깨진 패킷: 입력이 조용해질 때까지 버리고 NAK */
        {
            while (uart_getc_timeout(50) >= 0) {}
            if (++retries > 10) return -2;
            uart_putc(NAK);
            continue;
        }

        if ((uint8_t)blk == expect)
        {
            if (addr + size > FLASH_END || (*total + size) > max_len) { uart_putc(CAN); uart_putc(CAN); return -4; }
            for (uint32_t i = 0; i < size; i += 2)
            {
                if ((addr % PAGE_SIZE) == 0 && !flash_erase_page(addr)) { uart_putc(CAN); uart_putc(CAN); return -5; }
                uint16_t half = (uint16_t)(data[i] | (data[i + 1] << 8));
                if (!flash_program_half(addr, half)) { uart_putc(CAN); uart_putc(CAN); return -5; }
                addr += 2;
            }
            *total += size;
            expect++;
            started = 1; retries = 0;
            uart_putc(ACK);
        }
        else if ((uint8_t)blk == (uint8_t)(expect - 1)) { uart_putc(ACK); }     /* 중복 패킷(ACK 유실): 기록하지 않고 ACK */
        else { uart_putc(CAN); uart_putc(CAN); return -4; }                    /* 순서가 어긋남 */
    }
}

/* ---------------- 앱 검사/점프 (11_IAP_Bootloader 와 동일) ---------------- */
static int app_is_valid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4);
    return (sp >= SRAM_BEGIN && sp <= SRAM_END) && ((pc & 1u) == 1u) && (pc > APP_ADDR) && (pc < FLASH_END);
}

static void jump_to_app(void)
{
    uint32_t app_sp = *(volatile uint32_t *)APP_ADDR;
    uint32_t app_pc = *(volatile uint32_t *)(APP_ADDR + 4);
    __disable_irq();
    SysTick->CTRL = 0;
    for (int i = 0; i < 2; i++) { NVIC->ICER[i] = 0xFFFFFFFFu; NVIC->ICPR[i] = 0xFFFFFFFFu; }
    SCB->VTOR = APP_ADDR;
    __set_MSP(app_sp);
    ((void (*)(void))app_pc)();
    while (1) {}
}

int main(void)
{
    uart_init();
    ms_timer_init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);    /* LD2 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);    /* PC13 입력(풀업 선택) */
    GPIOC->BSRR = (1u << 13);

    uart_puts("\r\n[xmodem bootloader v1] app @ "); uart_hex(APP_ADDR); uart_puts("\r\n");

    int force = ((GPIOC->IDR & (1u << 13)) == 0);                /* B1 눌림 = 업데이트 모드 */
    if (!force && app_is_valid())
    {
        uart_puts("valid application -> jumping (hold B1 during reset to update)\r\n");
        uart_flush();
        jump_to_app();
    }

    uart_puts(force ? "B1 held: update mode\r\n" : "no valid application: update mode\r\n");
    uart_puts("send firmware.bin with XMODEM (CRC) within 60 s ...\r\n");
    GPIOA->ODR |= (1u << 5);                                      /* 업데이트 대기 중 LD2 켬 */

    uint32_t total = 0;
    flash_unlock();
    int rc = xmodem_receive(APP_ADDR, FLASH_END - APP_ADDR, &total);
    flash_lock();

    if (rc == 0)
    {
        uart_puts("\r\nreceived "); uart_dec(total); uart_puts(" bytes (last block is padded with 0x1A)\r\n");
        if (app_is_valid()) { uart_puts("application OK -> jumping\r\n"); uart_flush(); jump_to_app(); }
        uart_puts("received image is not a valid application (check that it is linked at 0x08008000)\r\n");
    }
    else
    {
        uart_puts("\r\ntransfer failed, code "); uart_dec((uint32_t)(-rc)); uart_puts(" (1=timeout 2=retries 3=cancelled 4=order/range 5=flash)\r\n");
    }

    while (1)                                                     /* 실패/무효: LD2 빠르게 점멸, 리셋해서 다시 시도 */
    {
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < 150000u; i++) {}
    }
}

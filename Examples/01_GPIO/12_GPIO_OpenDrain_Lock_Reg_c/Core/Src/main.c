/**
 * 12_GPIO_OpenDrain_Lock_Reg_c  —  [계층: 레지스터(CMSIS)]  오픈드레인 와이어드 AND와 LCKR 설정 잠금
 *
 * 교안 「GPIO·AFIO」 6.1절(출력 드라이버)·6.3절(잠금과 리셋 상태)의 실습이다.
 *
 * 실험 C — 오픈드레인 두 개로 와이어드 AND 만들기 (배선 필요)
 *   PA0(A0), PA1(A1)을 범용 오픈드레인 출력으로, PA4(A2)를 풀업 입력으로 두고 세 핀을 점퍼로 한 점에 묶는다.
 *   F1은 출력 모드에서 내부 풀업·풀다운이 꺼진다(RM0008 9.1.8). 그래서 오픈드레인 핀 자신의 풀업은 쓸 수 없고,
 *   같은 노드에 묶은 PA4의 약한 풀업(RPU 30~50 kΩ, 데이터시트 표 36)이 노드를 High로 끌어올린다.
 *   어느 한쪽이라도 0을 쓰면 N-MOS가 노드를 Low로 당긴다 → 노드 = ODR0 AND ODR1.
 *   오픈드레인 모드에서 IDR은 핀의 실제 레벨을 읽는다(RM0008 9.1.8). 그래서 PA0·PA1의 IDR도 ODR이 아니라 노드 값을 보인다.
 *   점퍼가 없으면 PA0·PA1은 1을 쓸 때 떠 있으므로 IDR 값이 정해지지 않는다(그것도 관찰 대상이다).
 *
 * 실험 D — LCKR로 핀 설정 잠그기 (리셋 전까지 되돌릴 수 없다. 리셋 직후 한 번만 실행)
 *   잠금 순서(RM0008 9.2.7): LCKR에 (LCKK=1 | LCK) 쓰기 → (LCKK=0 | LCK) 쓰기 → (LCKK=1 | LCK) 쓰기 → 읽기(LCKK=0) → 읽기(LCKK=1).
 *   이 순서 동안 LCK[15:0] 값이 바뀌면 잠금이 취소된다.
 *   PA0만 잠근 뒤 CRL의 PA0 니블(잠김)과 PA1 니블(안 잠김)을 바꿔 보고 다시 읽어 결과를 출력한다.
 *   잠금이 막는 것이 설정(CRL/CRH 4비트)인지, 출력 데이터(ODR)도 막는지 BSRR로 PA0을 써서 확인한다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB2 ÷1(64 MHz, GPIO)
 *
 * 사용 레지스터·비트
 *   GPIOA->CRL : PA0·PA1 = CNF 01(범용 오픈드레인) + MODE 10(2 MHz) → 니블 0x6
 *                PA4     = CNF 10(풀업/풀다운 입력) + MODE 00          → 니블 0x8, ODR4 = 1이면 풀업
 *   GPIOA->BSRR, GPIOA->IDR, GPIOA->ODR
 *   GPIOA->LCKR: LCK[15:0], LCKK(bit16)
 *
 * 인터럽트를 사용하지 않는다. 출력: USART2(PA2) 115200 8N1.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트(LATENCY = 010) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClockUpdate();
}

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < us * CYC_PER_US) {}
}

static void UART2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX: AF 푸시풀 */
    USART2->BRR = (PCLK1_HZ + 115200u / 2u) / 115200u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) uart_putc(b[--i]);
}
static void uart_puthex(uint32_t v, int digits)
{
    uart_puts("0x");
    for (int i = digits - 1; i >= 0; i--) uart_putc("0123456789ABCDEF"[(v >> (i * 4)) & 0xFu]);
}

static uint32_t nibble(uint32_t crl, uint32_t pin) { return (crl >> (pin * 4u)) & 0xFu; }

/* ---------------- 실험 C ---------------- */

static void WiredAnd_Run(void)
{
    uart_puts("C) wired-AND  (jumper A0-A1-A2; PA4 pull-up is the only pull-up on the node)\r\n");
    uart_puts("   ODR0 ODR1 | IDR0 IDR1 IDR4(node) | expect node\r\n");
    for (uint32_t k = 0; k < 4u; k++)
    {
        uint32_t o0 = (k >> 1) & 1u, o1 = k & 1u;
        GPIOA->BSRR = (o0 ? GPIO_BSRR_BS0 : GPIO_BSRR_BR0) | (o1 ? GPIO_BSRR_BS1 : GPIO_BSRR_BR1);
        delay_us(20);                                   /* 40 kΩ × 수십 pF 상승 시간보다 충분히 길게 */
        uint32_t idr = GPIOA->IDR;
        uart_puts("     "); uart_putu(o0); uart_puts("    "); uart_putu(o1);
        uart_puts("  |   "); uart_putu(idr & 1u);
        uart_puts("    "); uart_putu((idr >> 1) & 1u);
        uart_puts("      "); uart_putu((idr >> 4) & 1u);
        uart_puts("        |     "); uart_putu(o0 & o1); uart_puts("\r\n");
    }
}

/* ---------------- 실험 D ---------------- */

static uint32_t Lock_PA0(void)
{
    const uint32_t lck = GPIO_LCKR_LCK0;
    uint32_t r1, r2;
    GPIOA->LCKR = GPIO_LCKR_LCKK | lck;    /* ① LCKK = 1 쓰기 */
    GPIOA->LCKR = lck;                     /* ② LCKK = 0 쓰기 */
    GPIOA->LCKR = GPIO_LCKR_LCKK | lck;    /* ③ LCKK = 1 쓰기 */
    r1 = GPIOA->LCKR;                      /* ④ 읽기: LCKK = 0 */
    r2 = GPIOA->LCKR;                      /* ⑤ 읽기: LCKK = 1이면 잠김 확인 */
    (void)r1;
    return (r2 & GPIO_LCKR_LCKK) ? 1u : 0u;
}

static void Lock_Run(void)
{
    uart_puts("D) LCKR lock of PA0 (irreversible until reset)\r\n");
    uint32_t before = GPIOA->CRL;
    uint32_t ok = Lock_PA0();
    uart_puts("   lock sequence -> LCKK = "); uart_putu(ok);
    uart_puts("  LCKR = "); uart_puthex(GPIOA->LCKR, 5); uart_puts("\r\n");

    /* PA0·PA1 니블을 둘 다 범용 푸시풀(0x2)로 바꾸려고 시도한다 */
    GPIOA->CRL = (GPIOA->CRL & ~0xFFu) | 0x22u;
    uint32_t after = GPIOA->CRL;
    uart_puts("   CRL nibble PA0 (locked)  : "); uart_puthex(nibble(before, 0), 1);
    uart_puts(" -> write 0x2 -> "); uart_puthex(nibble(after, 0), 1); uart_puts("\r\n");
    uart_puts("   CRL nibble PA1 (unlocked): "); uart_puthex(nibble(before, 1), 1);
    uart_puts(" -> write 0x2 -> "); uart_puthex(nibble(after, 1), 1); uart_puts("\r\n");

    /* 출력 데이터는? PA0에 BSRR로 0과 1을 써 보고 ODR을 읽는다 */
    GPIOA->BSRR = GPIO_BSRR_BR0; uint32_t o_lo = GPIOA->ODR & 1u;
    GPIOA->BSRR = GPIO_BSRR_BS0; uint32_t o_hi = GPIOA->ODR & 1u;
    uart_puts("   PA0 ODR after BR0 / BS0  : "); uart_putu(o_lo); uart_puts(" / "); uart_putu(o_hi);
    uart_puts("   (does the lock freeze ODR too?)\r\n");

    /* PA1은 실험 C를 위해 오픈드레인(0x6)으로 되돌린다. PA0은 잠겨 있으므로 그대로 오픈드레인이다 */
    GPIOA->CRL = (GPIOA->CRL & ~0xF0u) | 0x60u;
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    UART2_Init();

    /* PA0·PA1 = 범용 오픈드레인 2 MHz(0x6), PA4 = 풀업 입력(0x8, ODR4 = 1), PA5 = LD2 푸시풀 2 MHz(0x2) */
    GPIOA->CRL = (GPIOA->CRL & ~((0xFu << 0) | (0xFu << 4) | (0xFu << 16) | (0xFu << 20)))
               | (0x6u << 0) | (0x6u << 4) | (0x8u << 16) | (0x2u << 20);
    GPIOA->BSRR = GPIO_BSRR_BS4;            /* CNF = 10 입력에서 ODR4 = 1 → 풀업 선택 */

    uart_puts("\r\n12_GPIO_OpenDrain_Lock_Reg_c\r\n");
    WiredAnd_Run();
    Lock_Run();
    uart_puts("   (reset the board to unlock)\r\n");

    for (;;)
    {
        delay_us(3000000u);
        WiredAnd_Run();
        GPIOA->BSRR = (GPIOA->ODR & (1u << 5)) ? GPIO_BSRR_BR5 : GPIO_BSRR_BS5;
    }
}

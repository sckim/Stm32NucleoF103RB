/**
 * 14_Clock_Gating_FlashWait_Reg_c  —  [계층: 레지스터(CMSIS)]  클럭 게이팅과 Flash 웨이트 스테이트를 실측
 *
 * 교안 「클럭 트리(RCC)」 6.3절(클럭 게이팅)과 6.4절(Flash 웨이트 스테이트)의 실습이다.
 *
 * 실험 A — 클럭 게이팅과 리셋 레지스터 (RM0008 7.3.5 RCC_APB1RSTR, 7.3.8 RCC_APB1ENR)
 *   주변장치 레지스터는 RCC의 클럭 허용 비트가 1일 때만 버스에 응답한다.
 *   TIM2->ARR(리셋값 0xFFFF)을 대상으로 다음 순서를 밟으며 매번 읽은 값을 출력한다.
 *     1) 클럭 끔 상태에서 0x1234 쓰기 → 읽기       : 기대 0x0000 (쓰기 무시, 읽기 0)
 *     2) 클럭 켬 → 읽기                              : 기대 0xFFFF (1)의 쓰기는 들어가지 않았다)
 *     3) 0x1234 쓰기 → 읽기                          : 기대 0x1234
 *     4) 클럭 끔 → 읽기                              : 기대 0x0000 (값은 남아 있지만 보이지 않는다)
 *     5) 클럭 다시 켬 → 읽기                         : 기대 0x1234 (클럭을 꺼도 레지스터 내용은 유지)
 *     6) APB1RSTR.TIM2RST 펄스 → 읽기               : 기대 0xFFFF (리셋은 내용을 지운다)
 *   "클럭이 꺼지면 읽은 값은 항상 0x0"은 RM0008 7.3.6 RCC_AHBENR 등의 Note에 적혀 있다.
 *   "꺼진 동안의 쓰기는 무시되고, 켜 두었던 내용은 유지된다"는 문서에 없는 기대이다. 실측으로 확인하는 것이 이 실험의 질문이다.
 *
 * 실험 B — Flash 웨이트 스테이트와 프리페치 (RM0008 3.3.3, FLASH_ACR)
 *   같은 함수의 실행 사이클을 DWT->CYCCNT로 잰다. 웨이트 스테이트는 "Flash 한 번 읽는 데 기다리는 SYSCLK 수"이다.
 *   SYSCLK 8 MHz(HSI)에서는 LATENCY = 0, 1, 2가 모두 허용되므로(많이 기다리는 것은 항상 안전하다)
 *   LATENCY 0·1·2 × 프리페치 끔·켬 6가지 조합을 모두 잰다.
 *   SYSCLK 64 MHz에서는 LATENCY = 2만 허용된다(48 < SYSCLK ≤ 72 MHz). 이 조합(LATENCY 2, 프리페치 켬) 하나만 잰다.
 *     ※ 64 MHz에서 LATENCY를 0이나 1로 낮추면 Flash에서 잘못된 명령어·데이터를 읽어 HardFault나 오동작이 난다.
 *       이 예제는 그런 설정을 절대 만들지 않는다. 직접 시험하지 말 것.
 *     ※ 프리페치 버퍼는 SYSCLK < 24 MHz이고 AHB 분주가 1일 때만 켜고 끌 수 있다(RM0008 3.3.3 "Reading the Flash memory").
 *       그래서 프리페치 끔·켬 비교는 8 MHz에서만 하고, 64 MHz로 올리기 전에 프리페치를 켜 둔다.
 *   측정 함수 두 개:
 *     bench_linear : 분기 없는 직선 코드(연속 주소 읽기) → 프리페치 버퍼가 웨이트를 숨기기 쉽다
 *     bench_branch : 분기·함수 포인터 호출이 많은 코드   → 분기할 때마다 프리페치 내용이 버려져 웨이트가 드러난다
 *
 * CMSIS 매크로 주의
 *   FLASH_ACR_LATENCY_0/_1/_2는 "웨이트 0/1/2"가 아니라 LATENCY[2:0]의 "비트 0/1/2"이다(값 0x1/0x2/0x4).
 *   2 웨이트(010)는 FLASH_ACR_LATENCY_1, 1 웨이트(001)는 FLASH_ACR_LATENCY_0이다. 이 예제는 숫자로 직접 쓴다.
 *
 * 클럭 전환 순서 (RM0008 7.2.6, 3.3.3)
 *   올릴 때: (8 MHz에서) 프리페치 켬 + LATENCY 먼저 2로 → APB1 ÷2 → PLL(HSI/2 × 16) 켜고 PLLRDY 대기 → SW = PLL → SWS 확인
 *   내릴 때: SW = HSI → SWS 확인 → PLL 끔 → APB1 ÷1 → LATENCY 낮춤
 *
 * 사용 레지스터·비트
 *   RCC->CR    : HSION(0), HSIRDY(1), PLLON(24), PLLRDY(25)
 *   RCC->CFGR  : SW[1:0], SWS[3:2], PPRE1[10:8], PLLSRC(16), PLLMULL[21:18]
 *   RCC->APB1ENR : TIM2EN(0), USART2EN(17)      RCC->APB1RSTR : TIM2RST(0)
 *   FLASH->ACR : LATENCY[2:0], PRFTBE(4), PRFTBS(5)
 *   DWT->CYCCNT, CoreDebug->DEMCR.TRCENA
 *
 * 인터럽트를 사용하지 않는다. 출력: USART2(PA2, ST-LINK 가상 COM) 115200 8N1. 배선 없음.
 */
#include "stm32f1xx.h"

static uint32_t g_sysclk = 8000000u;            /* 현재 SYSCLK (UART BRR·지연 계산용) */
static uint32_t g_pclk1  = 8000000u;            /* 현재 PCLK1 */

/* ---------------- UART (PCLK1이 바뀔 때마다 BRR 재계산) ---------------- */

static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_flush(void) { while (!(USART2->SR & USART_SR_TC)) {} }   /* 마지막 비트까지 송신 완료 */
static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) uart_putc(b[--i]);
}
static void uart_putu_w(uint32_t v, int w)
{
    uint32_t t = v; int d = 1;
    while (t >= 10u) { t /= 10u; d++; }
    while (d++ < w) uart_putc(' ');
    uart_putu(v);
}
static void uart_puthex16(uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int s = 12; s >= 0; s -= 4) uart_putc(hx[(v >> s) & 0xFu]);
}

static void uart_set_baud(void)
{
    USART2->BRR = (g_pclk1 + 115200u / 2u) / 115200u;   /* 8 MHz → 69, 32 MHz → 278 */
}

static void UART2_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;               /* USART2도 게이팅 대상: 켜야 레지스터가 응답한다 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 50 MHz */
    uart_set_baud();
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

/* ---------------- 클럭 전환 ---------------- */

static void Flash_Set(uint32_t latency, uint32_t prefetch)
{
    uint32_t acr = (FLASH->ACR & ~(FLASH_ACR_LATENCY | FLASH_ACR_PRFTBE)) | (latency & 7u);
    if (prefetch) acr |= FLASH_ACR_PRFTBE;
    FLASH->ACR = acr;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != (latency & 7u)) {}   /* 새 값이 반영됐는지 되읽어 확인 */
    /* 프리페치 상태(PRFTBS)는 버퍼가 실제로 켜졌는지를 알려 준다 */
    while (((FLASH->ACR & FLASH_ACR_PRFTBS) != 0u) != (prefetch != 0u)) {}
}

static void Clock_To64MHz(void)
{
    uart_flush();
    Flash_Set(2, 1);                                            /* 1) 웨이트를 먼저 올린다 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_PPRE1))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;       /* 2) HSI/2 × 16, APB1 ÷2 (≤ 36 MHz) */
    RCC->CR |= RCC_CR_PLLON;                                    /* 3) PLL 켜고 잠김 대기 */
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;   /* 4) SYSCLK ← PLL */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}   /* 5) 실제 전환 확인 */
    g_sysclk = 64000000u; g_pclk1 = 32000000u;
    uart_set_baud();
}

static void Clock_ToHSI8MHz(void)
{
    uart_flush();
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;   /* 1) SYSCLK ← HSI */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}
    RCC->CR &= ~RCC_CR_PLLON;                                   /* 2) PLL 정지 */
    RCC->CFGR &= ~RCC_CFGR_PPRE1;                               /* 3) APB1 ÷1 */
    g_sysclk = 8000000u; g_pclk1 = 8000000u;
    uart_set_baud();
    Flash_Set(0, 1);                                            /* 4) 웨이트는 맨 나중에 낮춘다 */
}

/* ---------------- DWT ---------------- */

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_ms(uint32_t ms)
{
    uint32_t cyc = (g_sysclk / 1000u) * ms;
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < cyc) {}
}

/* ---------------- 측정 대상 ---------------- */

static volatile uint32_t g_sink;

#define ADD8(x)  x += 3u; x ^= 0x55u; x += 7u; x ^= 0xAAu; x += 11u; x ^= 0x0Fu; x += 13u; x ^= 0xF0u;
#define ADD64(x) ADD8(x) ADD8(x) ADD8(x) ADD8(x) ADD8(x) ADD8(x) ADD8(x) ADD8(x)

/* 분기 없는 직선 코드: 명령어를 연속 주소에서 읽는다 */
__attribute__((noinline)) static uint32_t bench_linear(void)
{
    uint32_t x = g_sink;
    for (uint32_t i = 0; i < 50u; i++) { ADD64(x) ADD64(x) }
    return x;
}

/* 분기가 많은 코드: 함수 포인터 호출과 조건 분기로 명령어 흐름이 자주 끊긴다 */
__attribute__((noinline)) static uint32_t f0(uint32_t v) { return v + 1u; }
__attribute__((noinline)) static uint32_t f1(uint32_t v) { return v ^ 0x5Au; }
__attribute__((noinline)) static uint32_t f2(uint32_t v) { return (v << 1) | (v >> 31); }
__attribute__((noinline)) static uint32_t f3(uint32_t v) { return v - 3u; }
static uint32_t (*const kFn[4])(uint32_t) = { f0, f1, f2, f3 };

__attribute__((noinline)) static uint32_t bench_branch(void)
{
    uint32_t x = g_sink;
    for (uint32_t i = 0; i < 2000u; i++)
    {
        x = kFn[(x ^ i) & 3u](x);
        if (x & 0x10u) x += i; else x -= i;
    }
    return x;
}

static uint32_t measure(uint32_t (*fn)(void))
{
    uint32_t t0 = DWT->CYCCNT;
    g_sink = fn();
    return DWT->CYCCNT - t0;
}

/* ---------------- 실험 A ---------------- */

static void step(const char *label, uint32_t val, uint32_t expect)
{
    uart_puts("   ");
    uart_puts(label);
    uart_puts(" -> ARR = ");
    uart_puthex16(val);
    uart_puts(val == expect ? "  (as expected)\r\n" : "  (differs from expectation)\r\n");
}

static void Test_Gating(void)
{
    uart_puts("A) clock gating on TIM2->ARR (reset value 0xFFFF)\r\n");
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;                                         /* 쓰기 완료를 보장하는 되읽기 */

    TIM2->ARR = 0x1234;
    step("1) clock OFF, write 0x1234, read", TIM2->ARR, 0x0000);

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; (void)RCC->APB1ENR;
    step("2) clock ON,  read               ", TIM2->ARR, 0xFFFF);

    TIM2->ARR = 0x1234;
    step("3) write 0x1234, read            ", TIM2->ARR, 0x1234);

    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN; (void)RCC->APB1ENR;
    step("4) clock OFF, read               ", TIM2->ARR, 0x0000);

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; (void)RCC->APB1ENR;
    step("5) clock ON again, read          ", TIM2->ARR, 0x1234);

    RCC->APB1RSTR |=  RCC_APB1RSTR_TIM2RST;                     /* 리셋 펄스: 1을 썼다가 */
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;                     /* 0으로 되돌린다 */
    step("6) TIM2RST pulse, read           ", TIM2->ARR, 0xFFFF);

    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
}

/* ---------------- 실험 B ---------------- */

static void Test_FlashWait(void)
{
    uart_puts("B) Flash wait states (cycles measured by DWT, lower is faster)\r\n");
    uart_puts("   SYSCLK  LATENCY  prefetch |  linear  |  branch\r\n");

    for (uint32_t lat = 0; lat <= 2u; lat++)
        for (uint32_t pf = 0; pf <= 1u; pf++)
        {
            Flash_Set(lat, pf);
            (void)measure(bench_linear);                        /* 한 번 돌려 둔 뒤 측정 */
            uint32_t cl = measure(bench_linear);
            uint32_t cb = measure(bench_branch);
            Flash_Set(0, 1);                                    /* 출력하는 동안은 기본값 */
            uart_puts("   8 MHz      ");
            uart_putu(lat);
            uart_puts(pf ? "       on   |" : "       off  |");
            uart_putu_w(cl, 8);
            uart_puts("  |");
            uart_putu_w(cb, 8);
            uart_puts("\r\n");
        }

    Clock_To64MHz();                                            /* LATENCY 2, 프리페치 켬 상태로 올라간다 */
    {
        (void)measure(bench_linear);
        uint32_t cl = measure(bench_linear);
        uint32_t cb = measure(bench_branch);
        uart_puts("  64 MHz      2       on   |");
        uart_putu_w(cl, 8);
        uart_puts("  |");
        uart_putu_w(cb, 8);
        uart_puts("\r\n");
    }
    uart_puts("   (64 MHz: LATENCY 0/1 not allowed; prefetch may be toggled only below 24 MHz)\r\n");
    Clock_ToHSI8MHz();
}

int main(void)
{
    /* 리셋 직후: SYSCLK = HSI 8 MHz, FLASH_ACR = 0x30 (LATENCY 0, 프리페치 켜짐) */
    DWT_Init();
    UART2_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */

    uart_puts("\r\n14_Clock_Gating_FlashWait_Reg_c  (reset: SYSCLK = HSI 8 MHz, FLASH_ACR = ");
    uart_puthex16(FLASH->ACR);
    uart_puts(")\r\n");

    for (uint32_t round = 1;; round++)
    {
        uart_puts("[round ");
        uart_putu(round);
        uart_puts("]\r\n");
        Test_Gating();
        Test_FlashWait();
        GPIOA->ODR ^= (1u << 5);
        delay_ms(5000);
    }
}

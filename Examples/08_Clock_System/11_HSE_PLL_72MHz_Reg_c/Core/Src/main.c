/**
 * 11_HSE_PLL_72MHz_Reg_c  —  [계층: 레지스터(CMSIS)]  HSE(바이패스) + PLL x9 = 72MHz (STM32F103 최대 클럭), 실패 시 HSI 로 안전 복귀
 *
 * 다른 예제는 HSI/2 x16 = 64MHz 를 쓴다. F103 의 최대 SYSCLK 는 72MHz 이고, 그 값은 HSE 8MHz x 9 로 만든다.
 * (HSI/2 를 PLL 에 넣으면 최대 4MHz x 16 = 64MHz 까지만 가능하다)
 *
 * 클럭 경로 (RM0008 7.2)
 *   HSE 8MHz (ST-LINK MCO 가 공급, 바이패스 입력) --PLLXTPRE=0(/1)--> PLL x9 = 72MHz --> SYSCLK
 *   AHB /1 = 72MHz,  APB1 /2 = 36MHz (최대 36MHz),  APB2 /1 = 72MHz
 *   FLASH 웨이트 스테이트 2 (48MHz 초과 ~ 72MHz),  프리페치 버퍼 켬
 *
 * 하드웨어 대응
 *   RCC->CR    : HSEON(16), HSEBYP(18), HSERDY(17),  PLLON(24), PLLRDY(25)
 *   RCC->CFGR  : PLLSRC(16)=1(HSE), PLLXTPRE(17)=0, PLLMULL[21:18]=0111(x9), PPRE1[10:8]=100(/2), SW[1:0]=10(PLL), SWS[3:2] 확인, MCO[26:24]
 *   FLASH->ACR : LATENCY[2:0]=010, PRFTBE(4)=1
 *
 * 안전 장치
 *   HSE 는 보드에 따라 없을 수 있다(MB1136 C-01: HSE 미사용, C-02 이상: ST-LINK MCO). HSERDY 를 타임아웃으로 기다려서
 *   실패하면 PLL 을 시도하지 않고 HSI 8MHz 로 계속 동작한다 -> "HSE 를 기다리다 영원히 멈추는" 고장을 피한다.
 *   (클럭 장애를 동작 중에도 감지하려면 CSS(클럭 보안 시스템, RCC_CR.CSSON, 7.2.7)를 켜고 NMI 에서 처리)
 *
 * 관찰: LD2 가 0.5초 주기로 점멸(SysTick 1ms 기준 — SystemCoreClock 값으로 재설정하므로 클럭과 무관하게 0.5초),
 *       UART(115200)에 실제 클럭 소스와 SYSCLK 출력. MCO(PA8)에는 PLL/2 = 36MHz(성공 시) 또는 HSI 8MHz(실패 시)를 출력하므로
 *       스코프로 클럭을 교차 검증할 수 있다 (MCO 최대 50MHz 이므로 72MHz 는 직접 내보내지 않는다).
 *
 * ISR: SysTick_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

static volatile uint32_t g_ms;
void SysTick_Handler(void) { g_ms++; }

static int clock_to_72mhz(void)
{
    /* 1) HSE 바이패스 켜기 + 안정화 대기(타임아웃) */
    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    uint32_t t = 200000;
    while (!(RCC->CR & RCC_CR_HSERDY) && --t) {}
    if (!(RCC->CR & RCC_CR_HSERDY))
    {
        RCC->CR &= ~RCC_CR_HSEON;                              /* 실패: HSE 끄고 HSI 로 계속 */
        RCC->CR &= ~RCC_CR_HSEBYP;
        return 0;
    }

    /* 2) 플래시 웨이트 스테이트 2 + 프리페치 (SYSCLK 를 올리기 "전에") */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    /* 3) 버스 분주: AHB /1, APB1 /2 (36MHz), APB2 /1 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) | RCC_CFGR_PPRE1_DIV2;

    /* 4) PLL: 소스 HSE, HSE 분주 없음, x9 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL))
              | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    t = 200000;
    while (!(RCC->CR & RCC_CR_PLLRDY) && --t) {}
    if (!(RCC->CR & RCC_CR_PLLRDY))
    {
        RCC->CR &= ~RCC_CR_PLLON;                              /* PLL 실패: 웨이트 스테이트를 되돌리고 HSI 유지 */
        RCC->CR &= ~RCC_CR_HSEON;
        FLASH->ACR &= ~FLASH_ACR_LATENCY;
        RCC->CFGR &= ~RCC_CFGR_PPRE1;
        return 0;
    }

    /* 5) SYSCLK 를 PLL 로 전환하고 SWS 로 확인 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    t = 200000;
    while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) && --t) {}
    return ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL);
}

/* ---------------- USART2 (레지스터), 현재 APB1 클럭에 맞춰 BRR 계산 ---------------- */
static void uart_init(uint32_t pclk1)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFu << 8)) | (0x4Bu << 8);           /* PA2 AF-PP, PA3 입력 */
    USART2->BRR = (pclk1 + 3600u) / 7200u;                              /* USARTDIV x16 = pclk1 / (115200/16) */
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

int main(void)
{
    int pll_ok = clock_to_72mhz();
    SystemCoreClockUpdate();                                            /* CMSIS: RCC 를 읽어 SystemCoreClock 갱신 */
    uint32_t pclk1 = pll_ok ? SystemCoreClock / 2 : SystemCoreClock;

    uart_init(pclk1);
    uart_puts("\r\n[HSE + PLL x9] ");
    uart_puts(pll_ok ? "OK: SYSCLK = " : "HSE not available -> staying on HSI, SYSCLK = ");
    uart_dec(SystemCoreClock / 1000000u);
    uart_puts(" MHz, APB1 = ");
    uart_dec(pclk1 / 1000000u);
    uart_puts(" MHz\r\n");

    /* MCO(PA8): 성공하면 PLL/2 (36MHz), 실패하면 HSI (8MHz) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~0xFu) | 0xBu;                           /* PA8 AF-PP 50MHz */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_MCO) | (pll_ok ? RCC_CFGR_MCO_PLLCLK_DIV2 : RCC_CFGR_MCO_HSI);

    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);           /* LD2 */
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;                       /* 실제 SYSCLK 기준 1ms */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    uint32_t t_led = 0;
    while (1)
    {
        if ((uint32_t)(g_ms - t_led) >= 500u)
        {
            t_led += 500u;
            GPIOA->ODR ^= (1u << 5);
        }
    }
}

/**
 * 04_Clock_Config_Reg_c  —  [계층: 레지스터(CMSIS)]  RCC/FLASH 레지스터로 시스템 클럭 전환
 *
 * HAL 을 사용하지 않는다. B1(PC13)을 누를 때마다 SYSCLK 를 HSI 8MHz <-> PLL 48MHz 로 전환한다.
 *
 * 관찰 방법
 *   1) LD2(PA5) 점멸 속도: 바쁜 대기(for 루프) 기반이므로 48MHz 에서 6배 빠르게 깜빡인다.
 *   2) MCO 출력(PA8 = Nucleo 'D7')에 오실로스코프/로직 분석기: SYSCLK 를 그대로 내보낸다 (8MHz <-> 48MHz).
 *      (MCO 최대 50MHz 이므로 64MHz 대신 48MHz 를 사용)
 *
 * 클럭 경로 (RM0008 7장)
 *   HSI 8MHz --(/2)--> PLL x12 = 48MHz --> SYSCLK --> AHB(/1)=48MHz --> APB1(/2)=24MHz, APB2(/1)=48MHz
 *
 * 전환 순서 (순서가 틀리면 플래시 접근 오류/하드폴트)
 *   올릴 때: FLASH 웨이트 스테이트 증가 -> APB1 분주 -> PLL 켜고 대기 -> SW 를 PLL 로 -> SWS 확인
 *   내릴 때: SW 를 HSI 로 -> SWS 확인 -> PLL 끄기 -> FLASH 웨이트 스테이트 감소
 *
 * 인터럽트를 사용하지 않는다 (폴링 방식).
 */
#include "stm32f1xx.h"

static void clock_use_hsi_8mhz(void)
{
    RCC->CR |= RCC_CR_HSION;                             /* CR.HSION(bit0) */
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}                /* CR.HSIRDY(bit1) 대기 */

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;      /* CFGR.SW[1:0] = 00 */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}      /* CFGR.SWS[3:2] 로 실제 전환 확인 */

    RCC->CR &= ~RCC_CR_PLLON;                            /* CR.PLLON(bit24) = 0 : PLL 정지 */
    RCC->CFGR &= ~RCC_CFGR_PPRE1;                        /* APB1 분주 /1 */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;                    /* ACR.LATENCY[2:0] = 000 (0 WS, <=24MHz) */
}

static void clock_use_pll_48mhz(void)
{
    /* 1) 플래시 웨이트 스테이트: 24~48MHz -> 1WS. 프리페치 버퍼(PRFTBE) 켬 */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_1 | FLASH_ACR_PRFTBE;

    /* 2) PLL 소스/배수: PLLSRC(bit16)=0 -> HSI/2, PLLMULL[21:18] = 1010 -> x12 */
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= RCC_CFGR_PLLMULL12;

    /* 3) APB1 는 최대 36MHz -> /2 = 24MHz. SYSCLK 를 올리기 전에 미리 설정 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE1) | RCC_CFGR_PPRE1_DIV2;

    /* 4) PLL 시작 후 잠금 대기 */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}                /* CR.PLLRDY(bit25) */

    /* 5) SYSCLK 소스 전환 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;      /* SW = 10 */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
}

int main(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    /* PA5 = LD2 출력 푸시풀 2MHz : CRL[23:20] = 0x2 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);

    /* PA8 = MCO : 대체기능 푸시풀 50MHz. CRH[3:0] = CNF8(10) MODE8(11) = 0xB */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 0)) | (0xBu << 0);
    /* CFGR.MCO[26:24] = 100 : SYSCLK 출력 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_MCO) | RCC_CFGR_MCO_SYSCLK;

    /* PC13 = B1 입력 : CRH[23:20] = CNF13(10) MODE13(00) = 0x8, 풀업 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);
    GPIOC->BSRR = (1u << 13);

    int fast = 0;
    clock_use_hsi_8mhz();

    while (1)
    {
        GPIOA->ODR ^= (1u << 5);                         /* LD2 토글 */
        for (volatile uint32_t i = 0; i < 400000u; i++) {}   /* 사이클 소모 지연: 클럭이 빠를수록 짧아진다 */

        if ((GPIOC->IDR & (1u << 13)) == 0)              /* 눌림 (active low) */
        {
            fast = !fast;
            if (fast) clock_use_pll_48mhz();
            else      clock_use_hsi_8mhz();

            /* 버튼이 떨어질 때까지 대기 + 채터링 방지용 지연 */
            while ((GPIOC->IDR & (1u << 13)) == 0) {}
            for (volatile uint32_t i = 0; i < 200000u; i++) {}
        }
    }
}

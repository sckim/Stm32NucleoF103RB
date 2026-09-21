/**
 * 73_AFIO_Remap_Reg_c  —  [계층: 레지스터(CMSIS)]  AFIO 로 JTAG 핀 해방 + 타이머 채널 핀 리맵
 *
 * STM32F1 만의 함정
 *   리셋 직후 PA13/PA14(SWDIO/SWCLK) 뿐 아니라 PA15(JTDI), PB3(JTDO), PB4(NJTRST)도 디버그 포트가 점유한다.
 *   이 핀들을 일반 GPIO/타이머 핀으로 쓰려면 AFIO->MAPR 의 SWJ_CFG 를 바꿔야 한다.
 *   또, 주변장치 신호(예: TIM2_CH2)는 AFIO 리맵으로 다른 핀에 옮길 수 있다.
 *
 * 이 예제가 하는 일 (HAL 미사용, 클럭은 리셋 기본값 HSI 8MHz)
 *   1) AFIO->MAPR.SWJ_CFG = 010 : JTAG-DP 비활성 + SW-DP 활성  ->  PA15, PB3, PB4 를 GPIO 로 해방
 *      (SWD 는 유지되므로 ST-Link 로 계속 다운로드/디버그 가능. 100(전부 끔)으로 하면 디버거가 끊겨 복구가 어렵다!)
 *   2) AFIO->MAPR.TIM2_REMAP = 11 (완전 리맵) : TIM2_CH1=PA15, CH2=PB3, CH3=PB10, CH4=PB11
 *      -> TIM2_CH2 가 PB3(Nucleo 'D3')에 1kHz / 듀티 25% PWM 으로 출력됨
 *   3) PB4(Nucleo 'D5')를 일반 GPIO 로 200ms 마다 토글  (JTAG 해방 전에는 NJTRST 가 점유)
 *   4) LD2(PA5)는 0.5s 하트비트
 *
 * 비교 실험: FREE_JTAG_PINS 를 0 으로 바꾸면 (1)(2)(3)을 하지 않고 PB3/PB4 가 JTAG 로 남아 PWM/토글이 나오지 않는다.
 *
 * 핵심 레지스터 (RM0008 9.4.2 AFIO_MAPR)
 *   bit[26:24] SWJ_CFG (쓰기 전용): 000 전체 SWJ | 001 JTAG-DP + SW-DP 이지만 NJTRST 해방 | 010 SW-DP 만 | 100 전부 끔
 *   bit[9:8]   TIM2_REMAP : 00 없음 | 01 부분1(CH1/ETR=PA15, CH2=PB3) | 10 부분2(CH3=PB10, CH4=PB11) | 11 완전
 *   AFIO 클럭: RCC->APB2ENR.AFIOEN (bit0) 를 먼저 켜야 MAPR 에 쓸 수 있다.
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "stm32f1xx.h"

#define FREE_JTAG_PINS 1

static void delay_loop(volatile uint32_t n) { while (n--) {} }

int main(void)
{
    /* 클럭: AFIO(APB2 bit0), GPIOA(bit2), GPIOB(bit3), TIM2(APB1 bit0) */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

#if FREE_JTAG_PINS
    /* SWJ_CFG 는 쓰기 전용 필드라 읽은 값에 의존하지 말고, 다른 필드는 보존하면서 SWJ_CFG 만 덮어쓴다 */
    uint32_t mapr = AFIO->MAPR;
    mapr &= ~AFIO_MAPR_SWJ_CFG;
    mapr |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;                 /* 010: JTAG 끔, SWD 유지 */
    mapr &= ~AFIO_MAPR_TIM2_REMAP;
    mapr |= AFIO_MAPR_TIM2_REMAP_FULLREMAP;                /* 11: 완전 리맵 -> CH2 = PB3 */
    AFIO->MAPR = mapr;
#endif

    /* PA5 = LD2 : 출력 푸시풀 2MHz.  CRL[23:20] = CNF(00) MODE(10) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);

    /* PB3 = TIM2_CH2 (리맵 후): 대체기능 푸시풀 50MHz.  CRL[15:12] = CNF(10) MODE(11) = 0xB */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 12)) | (0xBu << 12);
    /* PB4 = 일반 출력 푸시풀 2MHz.  CRL[19:16] = 0x2 */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 16)) | (0x2u << 16);

    /* TIM2: 8MHz / 8 = 1MHz 카운터, ARR = 1000-1 -> 1kHz */
    TIM2->PSC = 8 - 1;                                     /* TIM2->PSC */
    TIM2->ARR = 1000 - 1;                                  /* TIM2->ARR */
    TIM2->CCR2 = 250;                                      /* 듀티 25% */
    TIM2->CCMR1 = (TIM2->CCMR1 & ~TIM_CCMR1_OC2M)
                | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2      /* OC2M = 110 : PWM 모드 1 */
                | TIM_CCMR1_OC2PE;                         /* CCR2 프리로드 */
    TIM2->CCER |= TIM_CCER_CC2E;                           /* CC2E : CH2 출력 활성 */
    TIM2->EGR |= TIM_EGR_UG;                               /* 레지스터 값을 즉시 반영 */
    TIM2->CR1 |= TIM_CR1_CEN;                              /* 카운터 시작 */

    uint32_t tick = 0;
    while (1)
    {
        delay_loop(150000);                                /* 8MHz 에서 약 100ms 급 (대략치) */
        tick++;
        GPIOB->ODR ^= (1u << 4);                           /* PB4 토글 (JTAG 해방 시에만 동작) */
        if ((tick % 5) == 0) GPIOA->ODR ^= (1u << 5);      /* LD2 하트비트 */
    }
}

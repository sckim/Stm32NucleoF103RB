/**
 * 04_BlinkReg  —  [계층: 레지스터(CMSIS)]  HAL/LL/CubeMX 없이 레지스터를 직접 제어
 *
 * 01_BlinkArduino -> 02_BlinkHAL -> 03_BlinkLL -> 04_BlinkReg 시리즈의 마지막(가장 낮은 추상화).
 * HAL/LL 이 감춰 주던 "클록 켜기 -> 핀 모드 설정 -> 출력" 과정을 직접 쓴다.
 *
 * 사용하는 레지스터 / 비트 (RM0008)
 *   RCC_APB2ENR.IOPAEN (bit2)   : GPIOA 클록 공급. 켜지 않으면 GPIOA 레지스터 쓰기가 무시된다.
 *   GPIOA_CRL  [23:20]          : PA5 설정 (핀당 4비트 = CNF[1:0] + MODE[1:0])
 *                                 MODE=01(출력 10MHz), CNF=00(범용 푸시풀)
 *   GPIOA_BSRR BS5 / BR5        : PA5 를 HIGH / LOW 로 "원자적"으로 변경 (ODR 읽고-쓰기 불필요)
 *   SysTick (LOAD/VAL/CTRL)     : SysTick_Config() 가 설정, 1ms 마다 인터럽트
 *
 * ISR 과 콜백의 구분
 *   SysTick_Handler()  : 진짜 ISR (벡터 테이블에서 직접 호출). msTicks 만 1 증가시키고 즉시 반환.
 *   HAL 콜백은 없다   : HAL 을 쓰지 않으므로 HAL_IncTick/HAL_..._Callback 계층 자체가 없다.
 *   delay_ms()         : main 이 msTicks 가 증가하는 것을 폴링하며 기다림 (volatile 필수).
 */
#include "stm32f1xx.h"

// SysTick 기반 밀리초 카운터
volatile uint32_t msTicks = 0;

void SysTick_Handler(void)
{
    msTicks++;
}

void SysTick_Init(void)
{
    // SystemCoreClock는 startup 코드에서 이미 설정됨 (기본 64MHz 또는 HSI 기준)
    // 1ms마다 인터럽트 발생하도록 설정
    SysTick_Config(SystemCoreClock / 1000);
}

void delay_ms(uint32_t ms)
{
    uint32_t start = msTicks;
    while ((msTicks - start) < ms)
    {
        __NOP();
    }
}

void GPIO_LED_Init(void)
{
    // 1. GPIOA 클록 활성화 (APB2 버스)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 2. PA5를 출력 모드로 설정
    // CRL 레지스터는 PA0~PA7 제어 (각 핀당 4비트: CNF[1:0] + MODE[1:0])
    // PA5는 비트 위치 20~23에 해당
    GPIOA->CRL &= ~(0xF << (5 * 4)); // 기존 설정 초기화
    GPIOA->CRL |= (0x1 << (5 * 4));  // MODE = 01 (출력, 10MHz)
                                     // CNF  = 00 (범용 푸시풀 출력)
}

int main(void)
{
    SysTick_Init();
    GPIO_LED_Init();

    while (1)
    {
        // 3. PA5 HIGH (LED 켜기) - BSRR 사용
        GPIOA->BSRR = GPIO_BSRR_BS5;
        delay_ms(1000);

        // 4. PA5 LOW (LED 끄기)
        GPIOA->BSRR = GPIO_BSRR_BR5;
        delay_ms(1000);
    }
}

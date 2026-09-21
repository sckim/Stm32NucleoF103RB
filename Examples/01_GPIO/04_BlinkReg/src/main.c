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
        delay_ms(500);

        // 4. PA5 LOW (LED 끄기)
        GPIOA->BSRR = GPIO_BSRR_BR5;
        delay_ms(500);
    }
}

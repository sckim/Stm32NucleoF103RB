#include "stm32f1xx.h"

// SysTick 기반 밀리초 카운터
volatile uint32_t msTicks = 0;

void SysTick_Handler(void) { msTicks++; }

void SysTick_Init(void) {
  // SystemCoreClock는 startup 코드에서 이미 설정됨 (기본 64MHz 또는 HSI 기준)
  // 1ms마다 인터럽트 발생하도록 설정
  SysTick_Config(SystemCoreClock / 1000);
}

void GPIO_LED_Init(void) {
  // 1. GPIOA 클록 활성화 (APB2 버스)
  RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPCEN);

  // 2. PA5를 출력 모드로 설정
  // CRL 레지스터는 PA0~PA7 제어 (각 핀당 4비트: CNF[1:0] + MODE[1:0])
  // PA5는 비트 위치 20~23에 해당
  GPIOA->CRL &= ~(0xFU << (5 * 4)); // 기존 설정 초기화
  GPIOA->CRL |= (0x1U << (5 * 4));  // MODE = 01 (출력, 10MHz)
                                   // CNF  = 00 (범용 푸시풀 출력)

  GPIOC->CRH &= ~(0xFU << ((13-8) * 4)); // Mode 00
  GPIOC->CRH |= (0x8U << ((13-8) * 4));  // MODE = 10 (입력, 10MHz)
  GPIOC->ODR |= (0x1U << 13);       // PULL-UP 활성화
}

int main(void) {
  SysTick_Init();
  GPIO_LED_Init();

  while (1) {
    // 3. PA5 HIGH (LED 켜기) - BSRR 사용
    if (GPIOC->IDR & (0x1U << 13)) // PC13이 HIGH인지 확인
    {
      GPIOA->BSRR = GPIO_BSRR_BS5; // LED ON
    } else {
      GPIOA->BSRR = GPIO_BSRR_BR5; // LED OFF
    }
  }
}

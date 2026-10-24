/**
 * 07_ReadPinReg  —  [계층: 레지스터(CMSIS)]  버튼(PC13) 입력을 폴링해 LED(PA5) 제어
 *
 * 05_ReadPin(HAL) 과 같은 동작을 레지스터로 구현한 버전이다. HAL_GPIO_ReadPin 이
 * 결국 어떤 레지스터를 읽는지 비교해 보자.
 *
 * 사용하는 레지스터 / 비트 (RM0008)
 *   RCC_APB2ENR.IOPAEN, IOPCEN  : GPIOA, GPIOC 클록 공급
 *   GPIOA_CRL  [23:20]          : PA5  출력 (MODE=01, CNF=00 푸시풀)
 *   GPIOC_CRH  [23:20]          : PC13 입력 (MODE=00, CNF=10 풀업/풀다운 입력)
 *   GPIOC_ODR  bit13            : 입력 모드에서 1 = 풀업, 0 = 풀다운 선택
 *   GPIOC_IDR  bit13            : 버튼 상태 읽기 (B1 은 Active-Low: 눌리면 0)
 *   GPIOA_BSRR BS5 / BR5        : LED 켜기 / 끄기
 *
 * 인터럽트: SysTick_Handler()(ISR)가 msTicks 를 증가시키지만 이 예제의 입력 처리에는 쓰이지 않는다.
 *   버튼은 인터럽트가 아니라 main 루프에서 폴링한다 (인터럽트 방식은 06_ExtInt 참고).
 */
#include "stm32f1xx.h"

// SysTick 기반 밀리초 카운터
volatile uint32_t msTicks = 0;

void SysTick_Handler(void) { msTicks++; }

void SysTick_Init(void) {
  // SystemCoreClock는 startup 코드의 SystemInit()에서 설정됨
  // (PLL을 따로 설정하지 않았으므로 기본값은 HSI 8MHz)
  // 1ms마다 SysTick 인터럽트 발생하도록 설정 (SysTick->LOAD = SystemCoreClock/1000 - 1)
  SysTick_Config(SystemCoreClock / 1000);
}

void GPIO_LED_Init(void) {
  // 1. GPIOA, GPIOC 클록 활성화 (APB2 버스, RCC_APB2ENR의 IOPAEN·IOPCEN 비트)
  RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPCEN);

  // 2. PA5를 출력 모드로 설정
  // CRL 레지스터는 PA0~PA7 제어 (각 핀당 4비트: CNF[1:0] + MODE[1:0])
  // PA5는 비트 위치 20~23에 해당
  GPIOA->CRL &= ~(0xFU << (5 * 4)); // 기존 설정 초기화
  GPIOA->CRL |= (0x1U << (5 * 4));  // MODE = 01 (출력, 10MHz)
                                   // CNF  = 00 (범용 푸시풀 출력)

  // 3. PC13(B1 버튼)을 풀업 입력으로 설정
  // CRH 레지스터는 PC8~PC15 제어, PC13은 비트 위치 20~23에 해당
  GPIOC->CRH &= ~(0xFU << ((13-8) * 4)); // 기존 설정 초기화
  GPIOC->CRH |= (0x8U << ((13-8) * 4));  // MODE = 00 (입력 모드)
                                         // CNF  = 10 (풀업/풀다운 입력)
  GPIOC->ODR |= (0x1U << 13);       // 입력 모드에서 ODR13 = 1 → 풀업 선택 (0이면 풀다운)
}

int main(void) {
  SysTick_Init();
  GPIO_LED_Init();

  while (1) {
    // 4. PC13 입력 상태를 폴링하여 PA5(LED) 제어 - IDR 읽기, BSRR 쓰기
    //    B1은 Active-Low: 버튼을 누르지 않으면 HIGH, 누르면 LOW
    if (GPIOC->IDR & (0x1U << 13)) // PC13이 HIGH인지 확인 (버튼 안 눌림)
    {
      GPIOA->BSRR = GPIO_BSRR_BS5; // BSRR BS5: PA5 = HIGH → LED ON
    } else {
      GPIOA->BSRR = GPIO_BSRR_BR5; // BSRR BR5: PA5 = LOW → LED OFF (버튼 눌림)
    }
  }
}

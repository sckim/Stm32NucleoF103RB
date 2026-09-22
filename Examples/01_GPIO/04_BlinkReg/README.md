# 04_BlinkReg

> 계층: **레지스터 직접 제어(CMSIS)** (PlatformIO `framework = cmsis`, HAL/LL 없음)

`RCC`, `GPIOA->CRL`, `GPIOA->BSRR` 레지스터를 직접 다뤄 LD2(PA5)를 0.5초 간격으로 점멸한다. 딜레이도 SysTick 인터럽트로 직접 만든다.

## 핵심 학습 내용

- **GPIO 설정 3단계를 레지스터로 확인**
  1. `RCC->APB2ENR |= RCC_APB2ENR_IOPAEN` : GPIOA 클럭 허용
  2. `GPIOA->CRL` 의 PA5 필드(비트 23:20)를 `CNF=00`(범용 푸시풀) + `MODE=01`(출력)으로 설정. 핀당 4비트이므로 `5 * 4` 비트 시프트.
  3. `GPIOA->BSRR = GPIO_BSRR_BS5 / BR5` : 원자적 set/reset (`ODR`을 읽고-수정-쓰기 하지 않음)
- **SysTick 기반 `delay_ms`**: `SysTick_Config(SystemCoreClock / 1000)`으로 1ms 예외를 만들고, `SysTick_Handler`가 `msTicks`(`volatile`)를 증가시킨다. `(msTicks - start) < ms` 뺄셈 비교는 32비트 오버플로에도 안전하다.
- HAL이 내부에서 하는 일을 직접 해 보며 `02_BlinkHAL`, `03_BlinkLL`의 API가 무엇을 감싸는지 이해한다.

## ISR

`SysTick_Handler` (벡터 테이블의 weak 심볼 재정의). 콜백 개념은 없다.

## 관련 예제

`02_BlinkHAL`, `03_BlinkLL`과 비교. SysTick을 더 자세히 다루는 `08_Clock_System/05_SysTick_Reg_c`, 비트 밴딩 `09_BitBanding_Reg_c`.

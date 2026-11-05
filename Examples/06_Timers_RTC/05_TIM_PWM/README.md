# 05_TIM_PWM

> 계층: **HAL** + TIM

> CubeMX(`TIM_PWM.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

TIM1_CH1(PA8, Nucleo D7)과 TIM1_CH3(PA10, Nucleo D2)에 1kHz PWM을 출력하고 듀티를 0~100%로 왕복시킨다. 두 채널은 같은 듀티로 함께 변한다.

## 핵심 학습 내용

- **PWM 주파수/듀티 계산**: TIM1은 APB2에 연결되며 APB2 분주비가 1이므로 타이머 클럭은 64MHz(×1)이다. `PSC = 63` → 1MHz, `ARR = 999` → 1kHz, `CCR1`/`CCR3` = 듀티 카운트 → 듀티% = `CCR / (ARR+1) × 100`. (APB1의 TIM2~4는 분주비 2라서 ×2로 64MHz가 된다.)
- **레지스터 대응**: `CCMR1.OC1M = 110`, `CCMR2.OC3M = 110`(PWM 모드 1), `CCER.CC1E`/`CC3E = 1`(출력 활성), `TIM1->CCR1`/`CCR3`.
- **Advanced 타이머(TIM1)**: 출력이 나오려면 `BDTR.MOE = 1`이어야 한다. HAL에서는 `HAL_TIM_PWM_Start()`가 내부에서 `__HAL_TIM_MOE_ENABLE()`을 호출하므로 TIM2와 사용법이 같다. 레지스터로 구현할 때만 `TIM1->BDTR |= TIM_BDTR_MOE`를 직접 써야 한다. Break/DeadTime은 쓰지 않는다(`BDTR.DTG = 0`, `BKE = 0`). 상보 출력과 DeadTime은 `08_TIM1_DeadTime_Break_c`에서 다룬다.
- **출력 비교와의 관계**: `04_TIM_OutputCompare`의 Toggle 모드(`OCxM = 011`)는 "일치할 때 반전"이다. PWM 모드 1(`110`)은 "`CNT < CCR`이면 High, 아니면 Low"이다. 같은 비교기를 쓰지만, 모든 채널이 `ARR`(주파수)를 공유하고 듀티만 다르다. PWM 설정은 `OCxPE = 1`(CCR 프리로드)을 켜므로 듀티 변경은 다음 주기부터 반영된다.
- **하드웨어 PWM의 장점**: 파형은 타이머 하드웨어가 생성하므로 CPU는 `CCR1`/`CCR3`만 바꾸면 되고 인터럽트가 필요 없다.
- **서보 응용**: PSC=63, ARR=19999(20ms = 50Hz), `CCR1 = 1000~2000`(1~2ms 펄스).
- **핀 제약**: 온보드 LD2(PA5)는 타이머 채널이 아니라 PWM 불가 → 외부 LED가 필요하다. TIM1_CH1/CH3은 리맵 없이 PA8/PA10으로 나온다.

## 배선

`PA8(D7) ── 330Ω ── 외부 LED 1 ── GND`

`PA10(D2) ── 330Ω ── 외부 LED 2 ── GND`

## 동작

PA8과 PA10의 LED가 어두움→밝음→어두움으로 숨쉬듯 함께 변한다. `HAL_Delay(1)`마다 CCR1/CCR3을 ±1 하므로 0→100%에 약 2초가 걸린다. 코드 주석의 "1ms × 1000 = 1초"보다 긴 이유는 `HAL_Delay()`가 최소 대기를 보장하려고 1틱을 더하기 때문이다 (`HAL_Delay(1)` ≈ 2ms, `03_TIM_Overflow_Counter`에서 실측). 스코프/로직 분석기로 PA8/PA10 파형(1kHz, 듀티 변화)을 관찰할 수 있다.

## 인터럽트

사용하지 않는다.

## 관련 예제

이전 `04_TIM_OutputCompare`(출력 비교 기본), 다음 `06_TIM_InputCapture_c`(입력 쪽), `08_TIM1_DeadTime_Break_c`(상보 PWM), `11_TIM_Servo_c`(서보), `08_Clock_System/07_AFIO_Remap_Reg_c`(PWM 핀 리맵).

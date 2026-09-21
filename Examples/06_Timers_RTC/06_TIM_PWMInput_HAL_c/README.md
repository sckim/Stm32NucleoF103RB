# 06_TIM_PWMInput_HAL_c

> 계층: **HAL** + TIM — Claude 작성, 검토 전 (컴파일만 확인)

**PWM 입력 모드**: 하드웨어가 입력 신호의 주기와 듀티를 동시에 측정한다.

## 핵심 학습 내용

- **`03_TIM_InputCapture_HAL_c`와의 차이**: `03_TIM_InputCapture_HAL_c`는 CPU가 두 캡처 값을 빼서 주기를 계산했다. PWM 입력 모드는 이를 하드웨어로 자동화한다.
- **원리** (RM0008 15.3.6, TIM2, 입력 PA0 = TI1):
  - CH1: `TI1FP1` 상승 에지 캡처 → `CCR1` = **주기**, 그리고 슬레이브 모드 **리셋**이 상승 에지마다 카운터를 0으로 되돌린다.
  - CH2: `TI1FP2` 하강 에지 캡처(간접 선택, `CC2S=10`) → `CCR2` = **하이 구간 폭**.
  - 결과: 카운터가 매 주기 0에서 시작하므로 `CCR1`이 곧 주기, `CCR2`가 곧 펄스 폭. 나눗셈 한 번으로 듀티.
  - `TIM2->SMCR`: `SMS=100`(리셋 모드), `TS=101`(TI1FP1).
- **측정 범위**: 1MHz(1µs), 16비트 → 최저 약 15.3Hz. 카운트가 클수록 분해능이 좋으므로 입력 주파수에 맞춰 PSC를 조정.

## 배선 / 동작

- TIM3_CH1(PA6, D12)이 1kHz PWM 테스트 신호를 출력하고, **B1을 누를 때마다** 듀티가 25% → 50% → 75%로 바뀐다.
- `PA6 ── 점퍼 ── PA0`로 연결하면 TIM2가 주파수/듀티를 측정해 UART(115200)로 출력한다. 외부 신호는 PA0에 직접 연결.

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR) → `HAL_TIM_IRQHandler()` → `HAL_TIM_IC_CaptureCallback()`(이 파일). CH1 캡처 시점에 주기 한 개가 완성되고 이때 `CCR2`도 같은 주기의 하이 폭을 이미 담고 있다.

## 관련 예제

`03_TIM_InputCapture_HAL_c`(CPU 계산 방식), `07_TIM_Encoder_HAL_c`, `08_TIM_OnePulse_HCSR04_HAL_c`.

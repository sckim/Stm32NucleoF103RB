# 07_ADC_Potentiometer_PWM_c

> 계층: **HAL + DMA** — 검토 전 (PlatformIO 빌드만 확인)

> CubeMX(`ADC_Potentiometer_PWM.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

가변저항으로 LED 밝기를 조절한다. **센서 → 처리 → 액추에이터**를 ADC(입력)와 TIM PWM(출력)으로 가장 단순하게 보여 주는 예제이며, 이동 평균, 감마 보정, 히스테리시스(불감대)를 함께 익힌다.

## 핵심 학습 내용

- **입력**: PA1(ADC1_IN1) 연속 변환 + DMA 로 최신값을 항상 변수에 유지. main 이 20ms 마다 읽는다.
- **처리** (코드 순서):
  1. 8샘플 이동 평균(노이즈 억제, `filtered = sum >> 3`)
  2. **감마 보정**(밝기 ≈ 입력², 사람 눈은 밝기를 로그에 가깝게 느낌): `duty = filtered² / 16765` (0~1000). `filtered`가 129 이하이면 정수 나눗셈으로 듀티가 0이 된다.
  3. **히스테리시스(불감대)**: 감마 보정 후의 듀티(0~1000 눈금)가 이전 출력과 **3 이상** 차이 날 때만 `CCR1`을 갱신한다 (조건 `duty > last + 2` 또는 `duty + 2 < last`). 단, 0과 1000은 차이와 관계없이 바로 반영한다.
- **출력**: TIM2_CH1(PA0) 1kHz PWM, `CCR1 = 0..1000`. 정수 연산으로 `duty = adc² / 16765`.
- 온보드 LD2(PA5)는 PWM 채널이 아니어서 외부 LED 를 PA0 에 연결한다.

## 배선 / 동작

PA1 에 가변저항 가운데 단자(양끝 3.3V/GND), PA0 → 330Ω → LED → GND. UART(115200)로 0.5초마다 ADC 값과 듀티 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다 (ADC 는 DMA 가 갱신). DMA 인터럽트도 NVIC에서 허용하지 않으며, CubeMX가 이를 강제로 켜지 않도록 `.ioc`의 `NVIC.ForceEnableDMAVector`를 `false`로 두었다 (기본값 `true`로 다시 생성하면 변환마다 DMA 인터럽트가 걸린다).

## 관련 예제

`05_ADC_AnalogWatchdog_c`(연속 변환 + DMA 패턴), `06_Timers_RTC/05_TIM_PWM`.

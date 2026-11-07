# 06_Timers_RTC — 타이머와 RTC

STM32F103의 범용 타이머(TIM2~4)와 고급 타이머(TIM1)를 시간 기준, 출력 비교·PWM 출력, 입력 캡처, 엔코더, 원 펄스로 활용하고, RTC를 달력 시계로 쓴다.

예제 번호는 학습 순서이다: **시간 기준 → 출력(OC → PWM) → 입력(IC → PWM 입력) → 고급·응용 → RTC**.
14~16은 교안 「타이머(TIM): 카운터와 비교기로 시간을 만드는 장치」(강의자료 폴더)의 6.1~6.3절에 맞춰 추가한 레지스터 수준 보충 예제이다. 번호는 뒤에 붙였지만, 권장 순서는 **01 → 02 → 03 → 14 → 04 → 05 → 15 → 06 → 07 → 16 → 08~13**이다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_TIM_TimeBase](01_TIM_TimeBase) | HAL | 타이머 1ms 주기 인터럽트 |
| 02 | [02_TIM_TimeBase_Polling](02_TIM_TimeBase_Polling) | HAL | 01을 인터럽트 없이 UIF 폴링으로, 틱 누락 관찰 |
| 03 | [03_TIM_Overflow_Counter](03_TIM_Overflow_Counter) | HAL+레지스터 | 오버플로 횟수로 32비트 µs 시계, 읽기 경쟁 조건 |
| 04 | [04_TIM_OutputCompare](04_TIM_OutputCompare) | HAL | 출력 비교 Toggle/Timing 모드, CCR 전진으로 채널별 주파수 |
| 05 | [05_TIM_PWM](05_TIM_PWM) | HAL | PWM 출력, LED 밝기 |
| 06 | [06_TIM_InputCapture_c](06_TIM_InputCapture_c) | HAL | 입력 캡처로 주파수 측정 |
| 07 | [07_TIM_PWMInput_c](07_TIM_PWMInput_c) | HAL | PWM 입력 모드(주기+듀티) |
| 08 | [08_TIM1_DeadTime_Break_c](08_TIM1_DeadTime_Break_c) | HAL | 상보 PWM, 데드타임, 브레이크 |
| 09 | [09_TIM_Encoder_c](09_TIM_Encoder_c) | HAL | 엔코더 인터페이스 모드 |
| 10 | [10_TIM_OnePulse_HCSR04_c](10_TIM_OnePulse_HCSR04_c) | HAL+레지스터 | 원 펄스 + 입력 캡처로 초음파 거리 |
| 11 | [11_TIM_Servo_c](11_TIM_Servo_c) | HAL | 50Hz PWM, 1~2ms = 0~180° 서보 |
| 12 | [12_TIM_MotorDriver_TB6612_c](12_TIM_MotorDriver_TB6612_c) | HAL | H-브리지 모터: 20kHz PWM, 브레이크/코스트, 비상 정지 |
| 13 | [13_RTC_Calendar_c](13_RTC_Calendar_c) | HAL+레지스터 | 32비트 카운터 기반 달력 |
| 14 | [14_TIM_Preload_URS_Reg_c](14_TIM_Preload_URS_Reg_c) | 레지스터 | ARR 이중 버퍼(ARPE)와 UG/URS 함정을 실측 — 01~03 다음에 본다 |
| 15 | [15_TIM_CenterAligned_PWM_Reg_c](15_TIM_CenterAligned_PWM_Reg_c) | 레지스터 | 가장자리·중앙 정렬 PWM 비교, 경계 조건, 마스터/슬레이브 동기 시작 — 05 다음에 본다 |
| 16 | [16_TIM_IC_Filter_Overcapture_Reg_c](16_TIM_IC_Filter_Overcapture_Reg_c) | 레지스터 | 입력 캡처 디지털 필터(ICxF) 경계와 오버캡처(CCxOF) — 07 다음에 본다 |

## 이 그룹에서 배우는 것

- **타이머 공식**: `f_update = f_timer_clk / ((PSC+1) × (ARR+1))`. APB 분주비가 1이 아니면 타이머 클럭은 ×2가 된다 (APB1 32MHz → TIM2~4 = 64MHz).
- **타이머의 세 가지 역할**: 시간 기준(UEV, CNT) / 출력(OC, PWM) / 입력(IC, 엔코더).
- **같은 `CCRx` 레지스터의 두 얼굴**: 출력 비교에서는 "이 시각에 이 일을 하라"는 뜻이고, 입력 캡처에서는 "그 일이 이 시각에 일어났다"는 뜻이다.
- **PWM은 출력 비교의 한 모드(`OCxM = 110`)이다.** 04를 05 앞에 둔 이유이다.
- **하드웨어가 대신하는 일**: PWM 생성, 주기·듀티 측정(PWM 입력), 방향 판별(엔코더), 정확한 펄스 폭(원 펄스).
- **고급 타이머 전용 기능**: 상보 출력, 데드타임, 브레이크.
- **F1 RTC의 특징**: 달력 하드웨어 없이 32비트 초 카운터만 있다.

## 참고

여러 예제가 테스트 신호를 스스로 만든다. TIM3 PWM을 PA6로 내보내 PA0로 되먹임하는 방식이다 (`06_TIM_InputCapture_c`, `07_TIM_PWMInput_c`). 배선은 각 README를 참고한다.

### 번호 재배치 (2026-11-03)

| 옛 번호 | 새 번호 |
|---|---|
| 11_TIM_TimeBase_Polling | 02_TIM_TimeBase_Polling |
| — | 03_TIM_Overflow_Counter (신규) |
| — | 04_TIM_OutputCompare (신규) |
| 02_TIM_PWM | 05_TIM_PWM |
| 03_TIM_InputCapture_c | 06_TIM_InputCapture_c |
| 06_TIM_PWMInput_c | 07_TIM_PWMInput_c |
| 04_TIM1_DeadTime_Break_c | 08_TIM1_DeadTime_Break_c |
| 07_TIM_Encoder_c | 09_TIM_Encoder_c |
| 08_TIM_OnePulse_HCSR04_c | 10_TIM_OnePulse_HCSR04_c |
| 09_TIM_Servo_c | 11_TIM_Servo_c |
| 10_TIM_MotorDriver_TB6612_c | 12_TIM_MotorDriver_TB6612_c |
| 05_RTC_Calendar_c | 13_RTC_Calendar_c |

다른 그룹의 문서나 강의자료에 옛 번호가 남아 있으면 이 표를 보고 고친다.

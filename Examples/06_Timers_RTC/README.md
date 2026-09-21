# 06_Timers_RTC — 타이머와 RTC

STM32F103의 범용 타이머(TIM2~4)·고급 타이머(TIM1)를 시간 기준, PWM 출력, 입력 캡처, 엔코더, 원 펄스로 활용하고, RTC를 달력 시계로 쓴다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 50 | [50_TIM_TimeBase](50_TIM_TimeBase/README.md) | HAL | 타이머 1ms 주기 인터럽트 |
| 51 | [51_TIM_PWM_HAL_c](51_TIM_PWM_HAL_c/README.md) | HAL | PWM 출력, LED 밝기 |
| 52 | [52_TIM_InputCapture_HAL_c](52_TIM_InputCapture_HAL_c/README.md) | HAL | 입력 캡처로 주파수 측정 |
| 53 | [53_TIM1_DeadTime_Break_HAL_c](53_TIM1_DeadTime_Break_HAL_c/README.md) | HAL | 상보 PWM, 데드타임, 브레이크 |
| 54 | [54_RTC_Calendar_HAL_c](54_RTC_Calendar_HAL_c/README.md) | HAL+레지스터 | 32비트 카운터 기반 달력 |
| 55 | [55_TIM_PWMInput_HAL_c](55_TIM_PWMInput_HAL_c/README.md) | HAL | PWM 입력 모드(주기+듀티) |
| 56 | [56_TIM_Encoder_HAL_c](56_TIM_Encoder_HAL_c/README.md) | HAL | 엔코더 인터페이스 모드 |
| 57 | [57_TIM_OnePulse_HCSR04_HAL_c](57_TIM_OnePulse_HCSR04_HAL_c/README.md) | HAL+레지스터 | 원 펄스 + 입력 캡처로 초음파 거리 |

## 이 그룹에서 배우는 것

- **타이머 공식**: `f_update = f_timer_clk / ((PSC+1) × (ARR+1))`. APB 분주비가 1이 아니면 타이머 클럭은 ×2 (APB1 32MHz → TIM2~4 = 64MHz)
- 타이머의 세 가지 역할: 시간 기준 / 출력(OC, PWM) / 입력(IC, 엔코더)
- 하드웨어가 대신하는 일: PWM 생성, 주기·듀티 측정(PWM 입력), 방향 판별(엔코더), 정확한 펄스 폭(원 펄스)
- 고급 타이머 전용 기능: 상보 출력, 데드타임, 브레이크
- F1 RTC의 특징: 달력 하드웨어 없이 32비트 초 카운터만 있다

## 참고

테스트 신호는 TIM3 PWM을 PA6로 내보내 PA0로 되먹임하는 식으로 자급하는 예제가 많다 (`52`, `55`). 배선은 각 README 참고.

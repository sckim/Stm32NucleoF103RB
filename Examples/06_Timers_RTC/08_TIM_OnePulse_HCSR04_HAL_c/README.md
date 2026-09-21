# 08_TIM_OnePulse_HCSR04_HAL_c

> 계층: **HAL + 레지스터(OPM 비트)** — Claude 작성, 검토 전 (컴파일만 확인)

**원 펄스 모드(OPM)** 로 정확한 10µs 트리거를 만들고, **입력 캡처**로 에코 폭을 재서 HC-SR04 초음파 센서의 거리를 측정한다.

## 핵심 학습 내용

- **HC-SR04 프로토콜**: TRIG에 10µs 이상 High → 센서가 40kHz 초음파 8발 송출 → ECHO가 왕복 시간 동안 High. `거리[cm] = ECHO 폭[µs] / 58` (음속 343m/s, 왕복이므로 /2).
- **원 펄스 모드** (RM0008 15.3.9): `TIM3->CR1.OPM=1`이면 카운터가 다음 업데이트 이벤트에서 스스로 멈춘다(CEN 자동 클리어). PWM 모드 2 + OPM은 `CCR1` 지연 후 `(ARR-CCR1)` 폭의 펄스 **1개**를 낸다. 1MHz 카운터에서 `CCR1=2, ARR=12` → 2µs 지연 후 10µs 폭. CPU가 10µs를 바쁜 대기하지 않고 하드웨어가 폭을 만든다.
- **에코 폭 측정**: TIM2 CH1(상승 에지 캡처)과 CH2(하강 에지 캡처, 같은 TI1 입력) → `폭 = CCR2 - CCR1`. 1MHz 16비트 → 최대 65ms(약 11m) 측정, HC-SR04 유효 범위(2~400cm, 약 23ms)에 충분.
- 타이머 두 개를 역할 분담(출력 TIM3, 입력 TIM2)하는 구성.

## 배선 (Nucleo-F103RB)

| 신호 | 핀 |
|---|---|
| VCC / GND | 5V / GND |
| TRIG | PA6 (D12, TIM3_CH1) |
| ECHO | PA0 (A0, TIM2_CH1/CH2 입력 캡처) |

**주의: ECHO는 5V 신호이므로 저항 분압(예: 1kΩ + 2kΩ)으로 3.3V 이하로 낮춰서 연결할 것.**

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR) → `HAL_TIM_IRQHandler()` → `HAL_TIM_IC_CaptureCallback()`(이 파일). CH2 = 하강 에지 = 에코 종료 시점에 폭이 완성된다.

## 관련 예제

`03_TIM_InputCapture_HAL_c`(입력 캡처), `06_TIM_PWMInput_HAL_c`(PWM 입력 모드).

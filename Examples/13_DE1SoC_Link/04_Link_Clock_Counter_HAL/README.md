# 04_Link_Clock_Counter_HAL_c

> 계층: **HAL** + TIM (`03_Link_PWM_Meter_HAL_c` 기반)

**TIM1_CH2(PA9 = D8)** 에 256 Hz 클럭(주기 1/256 s, 듀티 50 %)을 낸다. DE1-SoC의 `fpga/lab4_top.v`가 이 클럭의 상승 엣지를 세어 **LEDR0~9에 하위 10비트(2진수)**, **HEX5~HEX0에 24비트 전체(16진수 6자리)** 로 표시한다.

## 배선

| Nucleo | DE1-SoC JP1 |
|---|---|
| D8 (PA9, TIM1_CH2) | 1번 핀 (GPIO_0[0], PIN_AC18) |
| GND | 12번 핀 (GND) |

D2(PA10)는 이 프로그램이 쓰지 않으므로 연결하지 않는다.

## 클럭 계산

- TIM1 클럭 64 MHz, `PSC = 249` → 카운터 클럭 256 kHz
- `ARR = 999` → 주기 1000 카운트 = 1/256 s (256 Hz), `CCR2 = 500` → 듀티 50 %
- `CCMR1.OC2M = 110`(PWM 모드 1), `CCER.CC2E = 1`, `BDTR.MOE = 1`(`HAL_TIM_PWM_Start`가 세트)
- 주파수를 바꾸려면 `PSC`만 바꾼다. 주파수 = 64 MHz / ((PSC+1) × 1000). `PSC = 63999` → 1 Hz, `PSC = 6399` → 10 Hz. `.ioc`의 Prescaler도 같이 고친다.

## 원본(03)에서 바뀐 것

| 영역 | 변경 |
|---|---|
| `.ioc`, `main.h`, `stm32f1xx_hal_msp.c` | 출력 핀 PA8(CH1)·PA10(CH3) → PA9(CH2). 레이블 `CLK_OUT` |
| `MX_TIM1_Init` | `Prescaler = 249`, `Pulse = 500`, 채널 2만 설정 |
| `USER CODE 2` | `HAL_TIM_PWM_Start`를 CH2에만 호출 |
| `USER CODE 3` | 듀티 변경 루프 삭제(할 일 없음) |

## 버튼으로 클럭 속도 전환

Nucleo의 파란 버튼 B1(PC13, 누르면 Low)을 `while(1)`에서 폴링한다. 누르는 동안 `PSC = 63999`로 클럭이 1 Hz(1초), 떼면 `PSC = 249`로 256 Hz(1/256초)가 된다. 버튼 상태가 바뀔 때만 `__HAL_TIM_SET_PRESCALER`로 PSC를 쓴 뒤 `HAL_TIM_GenerateEvent(UPDATE)`(`EGR.UG = 1`)로 업데이트 이벤트를 강제한다. PSC는 프리로드 레지스터라 이벤트 없이는 다음 주기 끝에서야 적용되는데, 1 Hz에서는 그 대기가 최대 1초이기 때문이다. 이벤트 때 `CNT`가 0으로 초기화되므로 전환 순간 파형 위상이 다시 시작한다(출력이 Low였다면 상승 엣지가 하나 생길 수 있다). `.ioc`, `main.h`, `MX_GPIO_Init`에 PC13 입력(`B1`)을 추가했다.

## 인터럽트

사용하지 않는다(버튼도 폴링). 파형은 타이머 하드웨어가 만들고 CPU는 시작과 버튼 확인만 한다.

## 기대 결과

1/256초(약 3.9 ms)마다 LEDR이 2진수로 1씩 올라가고(너무 빨라 하위 비트는 깜박임으로 보이고, 상위 비트만 눈으로 읽힌다) HEX0이 0, 1, 2, …, 9, A, …, F로 바뀐다. HEX0이 F에서 0으로 넘어갈 때 HEX1이 1 오른다. 1024번째 클럭에서 LEDR이 모두 꺼지고 HEX2~HEX0은 400이다(24비트 카운터는 계속 센다). SW0을 올리면 0으로 되돌아간다.
MCU가 내부 RC 발진기(HSI)를 쓰므로 주파수는 256 Hz에서 조금 벗어난다. 세는 값에는 영향이 없다.

## FPGA 쪽

`fpga/lab4_top.v`는 MCU 클럭을 FPGA의 클럭으로 쓰지 않는다. 2단 동기화기로 `CLOCK_50` 영역에 옮긴 뒤 엣지 검출기로 한 주기짜리 펄스를 만들고, 그 펄스로 카운터를 올린다. `pins.tcl`의 `MCU_CLK`가 PIN_AC18이다. Quartus 최상위 엔티티를 `lab4_top`으로 지정한다.

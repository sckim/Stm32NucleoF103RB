# 04_TIM_OutputCompare

> 계층: **HAL** + TIM

> CubeMX(`TIM_OutputCompare.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `06_TIM_InputCapture_c`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

**출력 비교(Output Compare, OC)**: 카운터가 `CNT == CCRx`가 되는 순간 하드웨어가 핀의 출력 상태를 바꾸거나(토글/High/Low) 인터럽트를 낸다. 다음 예제인 PWM(`05_TIM_PWM`)도 출력 비교의 한 모드(`OCxM = 110`)이다. 이 예제에서는 그보다 기본이 되는 **Toggle 모드**와 **Timing 모드**를 다룬다.

타이머 하나, 자유 계수 카운터 하나로 세 채널이 서로 다른 일을 한다.

| 채널 | 핀 | 모드 (`OCxM`) | 하는 일 |
|---|---|---|---|
| CH1 | PA0 (A0) | Toggle (`011`) | 0.25초마다 핀 반전 → **2Hz** 깜빡임 |
| CH2 | PA1 (A1) | Toggle (`011`) | B1을 누를 때마다 **5 → 10 → 20 → 2Hz** 순환 |
| CH3 | 없음 | Timing (`000`, Frozen) | 1초마다 인터럽트만 발생 → LD2 토글 + UART 보고 |

## 핵심 학습 내용

- **비교 원리**: `CNT == CCRx`가 되면 `SR.CCxIF`가 서고, `OCxM` 설정에 따라 출력 기준 신호 `OCxREF`가 바뀐다. `CCER.CCxE = 1`이면 이 신호가 핀으로 나간다.
- **타이머 설정**: TIM2 클럭 64MHz, `PSC = 6399` → 10kHz(0.1ms), `ARR = 0xFFFF` 자유 계수. 오버플로 주기는 6.5536초이다.
- **CCR 전진(advance) 기법**: 비교 인터럽트가 날 때마다 ISR에서 `CCRx += 간격`을 한다. 카운터는 멈추지 않고, 채널마다 "다음에 일어날 시각"만 따로 정한다.
  - `ARR = 0xFFFF`이므로 `uint16_t` 덧셈의 래핑이 카운터 래핑과 정확히 맞는다.
  - Toggle 출력 주파수는 `f = 10kHz / (2 × 간격)`이다. 예) 간격 2500 → 2Hz.
- **PWM과의 차이**: PWM은 모든 채널이 `ARR`, 즉 같은 주파수를 공유하고 듀티만 다르다. OC + CCR 전진 방식은 **채널마다 주파수가 다르다.** 대신 듀티는 50%로 고정이고, 반전마다 인터럽트가 필요하다.
- **Timing 모드**: 핀을 쓰지 않고 "정해진 시각의 인터럽트"로만 쓴다. 한 타이머로 여러 개의 소프트웨어 타이머를 만들 때 쓴다.
- **프리로드 끄기**: `HAL_TIM_OC_ConfigChannel()`은 `OCxPE`(CCR 프리로드)를 켜지 않는다. 그래서 ISR에서 쓴 CCRx 값이 즉시 다음 비교값이 된다. 반면 PWM 설정 함수는 `OCxPE = 1`로 켠다.
- **핀 충돌 회피**: TIM2_CH3의 기본 핀은 PA2인데, 이 핀은 USART2_TX로 쓰고 있다. `HAL_TIM_OC_Start_IT(CH3)`은 `CC3E = 1`로 출력까지 켜므로 쓰지 않는다. 대신 `__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_CC3)`로 **인터럽트만** 켠다. F1은 한 핀을 두 주변장치가 동시에 구동하면 충돌한다.

## 레지스터 설정값

시작할 때 UART로 출력되는 값과 같다.

| 레지스터 | 값 | 의미 |
|---|---|---|
| `TIM2->PSC` | 6399 | 64MHz / 6400 = 10kHz |
| `TIM2->ARR` | 0xFFFF | 자유 계수 |
| `TIM2->CCMR1` | 0x3030 | `OC1M = 011`, `OC2M = 011` (Toggle), `CCxS = 00` (출력) |
| `TIM2->CCMR2` | 0x0000 | `OC3M = 000` (Frozen = Timing) |
| `TIM2->CCER` | 0x0011 | `CC1E = 1`, `CC2E = 1` (CH3은 출력 없음) |
| `TIM2->DIER` | 0x000E | `CC1IE`, `CC2IE`, `CC3IE` |
| `TIM2->CCR1/2/3` | 2500 / 1000 / 10000 | 첫 비교 시각. 이후 ISR에서 전진한다 |

## 배선

`05_TIM_PWM`과 같은 배선이다.

`PA0(A0) ── 330Ω ── 외부 LED 1 ── GND`

`PA1(A1) ── 330Ω ── 외부 LED 2 ── GND`

## 동작

- LED1은 2Hz로, LED2는 5Hz로 깜빡인다. B1(PC13)을 누를 때마다 LED2가 10 → 20 → 2 → 5Hz로 바뀐다.
- LD2는 1초마다 토글된다 (CH3 Timing 모드).
- USART2(115200 8N1) 출력:

```
TIM2 Output Compare start (10kHz tick, CH1/CH2 toggle, CH3 timing)
CCMR1=0x3030 CCMR2=0x0000 CCER=0x0011 DIER=0x000E
[  1 s] CH1(PA0)  2.0 Hz  CH2(PA1)  5.0 Hz  | CNT=10003 CCR1=12500 CCR2=11000 CCR3=20000
B1: CH2(PA1) 10.0 Hz
[  2 s] CH1(PA0)  2.0 Hz  CH2(PA1) 10.0 Hz  | CNT=20002 CCR1=22500 CCR2=20500 CCR3=30000
...
```

(CNT·CCR 값은 예시이다. CCRx는 항상 CNT보다 조금 앞의 "다음 사건 시각"을 가리킨다.)

## 스코프로 볼 때

`PSC = 63`(1MHz)으로 바꾸고 `CH1_HALF = 500`, `kHalf2Table = {250, ...}`으로 하면 1kHz와 2kHz의 구형파가 나온다. 이렇게 하면 `07_TIM_PWMInput_c`나 로직 분석기로 주파수를 확인할 수 있다. 단, 이때 CH3 간격 10000은 10ms가 된다. 1초를 만들려면 콜백에서 100번을 세야 한다.

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR, `stm32f1xx_it.c`) → `HAL_TIM_IRQHandler()`가 `SR.CCxIF`를 지움. `CCMRx.CCxS = 00`(출력)이면 `HAL_TIM_OC_DelayElapsedCallback()`(이 예제 `main.c`)을 호출한다. 콜백은 `htim->Channel`로 채널을 구분해 CCRx만 전진시키고, 출력은 main 루프가 플래그를 보고 한다. 한 번 진입했을 때 여러 플래그가 서 있으면 CH1 → CH2 → CH3 순서로 콜백이 차례로 불린다.

## 생각해 볼 것

- 콜백에서 오래 머물면 어떻게 될까? 다음 비교 시각(CCR)을 CNT가 이미 지나쳐 버린다. 그러면 그 채널은 카운터가 한 바퀴(6.5초) 돌 때까지 멈춘다. **간격이 짧을수록 ISR 지연에 민감하다.**
- 50%가 아닌 듀티를 만들려면 어떻게 해야 할까? 반전할 때마다 High 간격과 Low 간격을 번갈아 더하면 된다. 다만 채널 하나에 듀티와 주파수를 모두 고정할 거라면 PWM(`05_TIM_PWM`)이 더 간단하다.

## 관련 예제

이전 `03_TIM_Overflow_Counter`(CNT를 시간으로 읽기), 다음 `05_TIM_PWM`(`OCxM = 110`, 하드웨어 듀티 생성), 비교 `10_TIM_OnePulse_HCSR04_c`(PWM 모드 2 + OPM으로 펄스 1개).

# 15_TIM_CenterAligned_PWM_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전

교안 「타이머(TIM)」 6.2절(출력 비교와 PWM)과 6.4절(TRGO·ITR 연결)의 실습이다.
같은 ARR·CCR로 **가장자리 정렬 PWM**과 **중앙 정렬 PWM**을 나란히 내고, 두 타이머를 **마스터/슬레이브로 동시에 출발**시켜 파형을 비교한다.

## 핵심 학습 내용

- **두 정렬 방식** (`CR1.CMS`)
  - 가장자리 정렬(`CMS = 00`): CNT가 0 → ARR로 오르고 0으로 돌아간다. 주기 = `ARR+1`, 듀티 = `CCR/(ARR+1)`.
  - 중앙 정렬(`CMS = 01`): CNT가 0 → ARR → 0을 오간다. 주기 = `2×ARR`, 듀티 = `CCR/ARR`. 펄스가 카운터 골짜기를 중심으로 대칭이 된다.
  - 같은 클럭이면 중앙 정렬의 PWM 주파수는 거의 절반이다(이 예제: 1 kHz vs 약 500.5 Hz).
- **PWM 모드 1의 경계 조건**: `CCR = 0`이면 계속 Low(0 %), `CCR > ARR`이면 계속 High(100 %). B1로 CCR을 0 → 250 → 500 → 999 → 1000으로 바꿔 확인한다.
- **`CMS`는 `CEN = 0`일 때만 바꾼다.** 중앙 정렬에서 `CR1.DIR`은 하드웨어가 갱신하는 읽기 전용 비트가 된다(UART 출력에서 DIR 값이 바뀌는 것을 볼 수 있다).
- **마스터/슬레이브 동기 시작**
  - TIM2(마스터): `CR2.MMS = 001` → CEN이 켜지면 TRGO가 High.
  - TIM3(슬레이브): `SMCR.TS = 001`(ITR1 = TIM2), `SMCR.SMS = 110`(트리거 모드) → TRGO 상승 에지에서 하드웨어가 TIM3의 CEN을 켠다.
  - ITR 번호는 고정 배선이다. RM0008 「TIMx internal trigger connection」 표에서 TIM3의 ITR1이 TIM2이다.

## 레지스터

| 레지스터 | TIM2 (PA0) | TIM3 (PA6) |
|---|---|---|
| `PSC` / `ARR` | 63 / 999 | 63 / 999 |
| `CR1` | `ARPE`, `CMS = 00` | `ARPE`, `CMS = 01` |
| `CR2` | `MMS = 001` (Enable → TRGO) | — |
| `SMCR` | — | `TS = 001`, `SMS = 110` |
| `CCMR1` | `OC1M = 110`, `OC1PE` | `OC1M = 110`, `OC1PE` |
| `CCER` | `CC1E` | `CC1E` |

## 배선 / 관찰

배선 없이 동작한다. AD2·오실로스코프 2채널로 **PA0(A0)**과 **PA6(D12)**을 함께 본다. LED로 보려면 각 핀에 330 Ω + LED를 단다.

- 가장자리 정렬 펄스는 주기 시작에 붙어 있고, 중앙 정렬 펄스는 대칭이다.
- CCR을 바꾸면 가장자리 정렬은 뒤쪽 에지만, 중앙 정렬은 양쪽 에지가 함께 움직인다.

## 동작

USART2(115200 8N1)로 B1을 누를 때마다 다음과 같이 출력한다.

```
15_TIM_CenterAligned_PWM_Reg_c  ARR=999, 1 count = 1 us
edge-aligned period = 1000 us (1 kHz), center-aligned period = 1998 us (500.5 Hz)
B1: CCR1 0 -> 250 -> 500 -> 999 -> 1000(>ARR)
CCR1=250  edge(PA0) duty 25.0%  | center(PA6) duty 25.0%   TIM3 CEN=1 DIR=0
CCR1=500  edge(PA0) duty 50.0%  | center(PA6) duty 50.0%   TIM3 CEN=1 DIR=1
...
```

## 생각해 볼 것

- 중앙 정렬로 1 kHz를 내려면 ARR을 얼마로 해야 하는가? 그때 듀티 분해능은 가장자리 정렬과 비교해 어떻게 되는가?
- 모터 구동에서 중앙 정렬을 쓰는 이유는 여러 채널의 스위칭 순간이 한 점에 몰리지 않기 때문이다. 3채널을 모두 중앙 정렬로 두고 서로 다른 CCR을 주면 에지가 어떻게 분포하는가?

## 인터럽트

사용하지 않는다.

## 관련 예제

`05_TIM_PWM`(HAL 가장자리 정렬 PWM), `08_TIM1_DeadTime_Break_c`(상보 PWM), `12_Advanced_Topics/05_TIM_Cascade32_c`(마스터/슬레이브로 32비트 카운터).

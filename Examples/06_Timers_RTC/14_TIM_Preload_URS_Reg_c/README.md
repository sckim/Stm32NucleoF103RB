# 14_TIM_Preload_URS_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전

교안 「타이머(TIM): 카운터와 비교기로 시간을 만드는 장치」 6.1절(시간 기반 유닛과 이중 버퍼)의 실습이다.
ARR 이중 버퍼(`CR1.ARPE`)와 UIF 발생원 선택(`CR1.URS`)이 왜 필요한지를 **결함을 직접 일으켜** 확인한다.

## 핵심 학습 내용

- **이중 버퍼**: `PSC`·`ARR`·`CCRx`는 소프트웨어가 쓰는 프리로드 레지스터와 실제 비교에 쓰이는 섀도 레지스터 두 겹이다. 둘 사이의 문은 갱신 이벤트(UEV)가 연다. `PSC`는 항상 버퍼링되고, `ARR`은 `ARPE`로 고른다.
- **실험 A — UG와 URS**: 초기화 끝의 `EGR.UG`는 섀도 레지스터를 채우는 정상 절차이다. 그런데 `URS = 0`이면 이 UG도 `SR.UIF`를 세우므로, `DIER.UIE`를 먼저 켜 두었다면 **카운터를 시작하기도 전에 인터럽트가 1번 들어온다.** `URS = 1`이면 오버플로만 UIF를 세운다.
- **실험 B — ARPE와 긴 주기**: TIM3(1 µs 카운트, 1 ms 주기)이 도는 중에 `CNT ≥ 700`에서 `ARR`을 999 → 499로 줄인다.
  - `ARPE = 1`: 현재 주기는 1000 µs로 끝나고 다음 주기부터 500 µs가 된다.
  - `ARPE = 0`: 새 ARR이 즉시 반영된다. 비교기는 `CNT == ARR`인 순간만 보므로, 이미 499를 지난 CNT는 0xFFFF까지 올라갔다가 0으로 넘어간 뒤에야 499를 만난다. 한 주기가 **약 65~66 ms**로 늘어난다.
- **측정 방법**: UEV마다 ISR이 `DWT->CYCCNT`(64 MHz 사이클 카운터)를 기록하고, main 루프가 차이를 µs로 출력한다.

## 레지스터

| 레지스터 | 값 | 의미 |
|---|---|---|
| `TIM3->PSC` | 63 | 64 MHz / 64 = 1 MHz |
| `TIM3->ARR` | 999 → 499 | 1 ms → 0.5 ms |
| `TIM3->CR1` | `URS`(bit2), `ARPE`(bit7), `CEN`(bit0) | 실험에 따라 바꾼다 |
| `TIM3->DIER` | `UIE`(bit0) | 갱신 인터럽트 |
| `TIM3->EGR` | `UG`(bit0) | 소프트웨어 UEV |

## 동작

배선은 필요 없다. 3초마다 두 실험을 반복하고 USART2(ST-LINK 가상 COM, 115200 8N1)로 출력한다. LD2는 라운드마다 토글된다.

```
14_TIM_Preload_URS_Reg_c  (SYSCLK 64 MHz, TIM3 clock 64 MHz, 1 count = 1 us)
[round 1]
  A) UG with UIE=1, CEN=0 : ISR entries  URS=0 -> 1 , URS=1 -> 0   (expect 1 , 0)
  B) ARR 999 -> 499 written at CNT>=700
  ARPE=0 : UEV period [us] = 1000 1000 66036 500 500 500 500 500 500
  ARPE=1 : UEV period [us] = 1000 1000 1000 500 500 500 500 500 500
```

(ARPE=0의 긴 주기 값은 예시이다. 0xFFFF→0 랩 지점에서 UEV가 나면 약 65 536, 다음 499 일치에서 나면 약 66 036이 나온다. **실측값으로 어느 쪽인지 확인하는 것이 이 실험의 질문이다.**)

## 생각해 볼 것

- PWM을 내는 중에 주파수를 바꾸면 이 긴 주기가 출력 파형에 그대로 나타난다. `05_TIM_PWM`에서 `ARPE`를 끄고 ARR을 줄이면 어떻게 보일까?
- 01·02·03 예제가 시작 직전에 UIF를 지우는 이유가 실험 A이다. URS=1과 "UG 뒤 UIF 지우기" 중 어느 쪽이 더 안전한가?

## ISR vs 콜백

`TIM3_IRQHandler`(진짜 ISR, 이 파일)가 `SR.UIF`를 지우고 횟수·시각만 기록한다. HAL 콜백은 없다.

## 관련 예제

`01_TIM_TimeBase`(HAL로 같은 1 ms 시간 기준), `02_TIM_TimeBase_Polling`·`03_TIM_Overflow_Counter`(시작 전 UIF 정리), `08_Clock_System/10_DWT_Profiling_Reg_c`(DWT 사이클 측정).

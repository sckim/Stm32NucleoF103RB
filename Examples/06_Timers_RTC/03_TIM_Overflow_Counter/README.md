# 03_TIM_Overflow_Counter

> 계층: **HAL** + TIM 레지스터 직접 읽기(`TIM2->CNT`, `TIM2->SR`)

> CubeMX(`TIM_Overflow_Counter.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `06_TIM_InputCapture_c`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

01·02에서는 오버플로(업데이트 이벤트)를 **"주기 신호"**로만 썼다. 이 예제는 **카운터 값(`CNT`) 자체를 시간으로 읽는다.** 16비트 카운터에 오버플로 횟수를 붙여 **32비트 µs 시계**로 확장하고, 두 값을 따로 읽을 때 생기는 **경쟁 조건(race condition)**을 관찰한다.

## 핵심 학습 내용

- **자유 계수 카운터**: TIM2 클럭 64MHz, `PSC = 63` → 1MHz(1µs), `ARR = 0xFFFF` → 65.536ms마다 오버플로.
  `f_update = 64MHz / ((63+1) × (65535+1)) = 15.26Hz`
- **카운터 확장**: 업데이트 ISR이 `gOvf++` → `시간[µs] = (gOvf << 16) | CNT`. 32비트이므로 약 71.6분마다 한 바퀴 돈다.
- **래핑에 안전한 비교**: `(int32_t)(a - b) < 0`이면 a가 b보다 이전이다. 32비트가 한 바퀴 돌아도 맞는 비교이다.
- **경쟁 조건**: `gOvf`와 `CNT`는 서로 다른 두 번의 읽기이다. 그 사이에 오버플로가 나면 상위값은 옛 값, 하위값은 새 값(0 근처)이 되어 **시간이 65536µs 뒤로 튄다.**
- **안전한 읽기 패턴** (`Micros_Safe()`):
  1. 인터럽트를 막는다 (`PRIMASK` 저장 후 `__disable_irq()`).
  2. `gOvf`, `CNT`를 읽는다.
  3. `SR.UIF = 1`이고 `CNT < 0x8000`이면, 아직 ISR이 세지 못한 오버플로이므로 상위값에 1을 더한다.
  4. `PRIMASK`를 복원한다.
- **시작 전 플래그 정리**: `HAL_TIM_Base_Init()`의 `EGR.UG` 때문에 UIF가 남아 있을 수 있다. 그대로 두면 시작하자마자 `gOvf`가 1이 되므로, 시작 전에 UIF를 지우고 `CNT`를 0으로 만든다.
- **µs 시계의 응용**: 코드 실행 시간 측정(printf 한 줄), `HAL_Delay()`의 실제 길이 측정.

## 두 읽기 함수 비교

| 항목 | `Micros_Naive()` | `Micros_Safe()` |
|---|---|---|
| 읽기 순서 | `gOvf` → `CNT` | 인터럽트 차단 → `gOvf` → `CNT` → `SR.UIF` 확인 → 복원 |
| 두 읽기 사이 오버플로 | 상위값이 1 작다 → 65536µs 뒤로 튐 | UIF로 감지해 보정 |
| 인터럽트 지연 | 없음 | 수 사이클 동안 지연 |
| ISR 안에서 호출 | 가능 | 가능 (PRIMASK 복원) |

## 동작

배선은 필요 없다. LD2(PA5)가 1초마다 토글되고, USART2(ST-LINK 가상 COM, 115200 8N1)로 매초 다음과 같이 출력된다.

```
TIM2 overflow counter start (1us tick, 16bit CNT + overflow count = 32bit us)
[  1 s] t=   1010312 us ovf=   15 | back-jumps naive=0 safe=0 | HAL_Delay(10)=10842 us | prev printf=0 us
[  2 s] t=   2030145 us ovf=   30 | back-jumps naive=1 safe=0 | HAL_Delay(10)=10377 us | prev printf=9510 us
...
```

(값은 예시이다.)

- `back-jumps naive`: 1초 동안 `Micros_Naive()`가 이전 값보다 작은 값을 돌려준 횟수. 기본 설정(`NAIVE_GAP_NOPS = 0`)에서는 두 읽기 사이가 몇 사이클뿐이라 **수 초에 한 번** 정도 나온다.
- `back-jumps safe`: 항상 0이어야 한다.
- `HAL_Delay(10)`: 10~11ms 사이 값이 나온다. HAL_Delay는 "최소 10ms"를 보장하려고 1틱을 더 기다리기 때문이다.
- `prev printf`: 직전 줄을 출력하는 데 걸린 시간이다. 약 110자 × 10비트 ÷ 115200bps ≈ **9.5ms**. 02_TIM_TimeBase_Polling에서 "printf가 루프를 막는다"고 한 시간을 직접 잰 값이다.

## 실험

1. `main.c`의 `NAIVE_GAP_NOPS`를 `50`으로 바꾼다. 두 읽기 사이가 길어지면 `naive` 횟수가 매초 여러 번으로 늘어난다. **창이 짧으면 드물게 틀릴 뿐, 틀린다는 사실은 그대로이다.**
2. 32비트 대신 64비트 시계가 필요하면 `uint64_t`로 `(gOvf << 16) | CNT`를 계산하면 된다. 이때 `gOvf`(32비트)를 한 번에 읽는 것은 Cortex-M3에서 원자적이다.
3. 이 확장 기법을 입력 캡처에 쓰면, `06_TIM_InputCapture_c`의 측정 하한(15.3Hz)보다 느린 신호도 잴 수 있다. 캡처 값과 오버플로 횟수를 함께 쓰면 된다.

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR, `stm32f1xx_it.c`) → `HAL_TIM_IRQHandler()`가 `SR.UIF`를 지움 → `HAL_TIM_PeriodElapsedCallback()`(이 예제 `main.c`)은 `gOvf++` 한 줄로 끝낸다. 출력은 모두 main 루프에서 한다.

## 관련 예제

이전 `02_TIM_TimeBase_Polling`(UIF 폴링, 틱 누락), 다음 `04_TIM_OutputCompare`(CNT와 CCR의 비교), 응용 `06_TIM_InputCapture_c`(캡처 시각 차로 주기 측정).

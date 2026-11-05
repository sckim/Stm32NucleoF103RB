# 02_TIM_TimeBase_Polling

> 계층: **HAL** + TIM 플래그 매크로(`__HAL_TIM_GET_FLAG` / `__HAL_TIM_CLEAR_FLAG`) (STM32CubeIDE/CubeMX 프로젝트: `Src`, `Inc`, `.ioc`)

[01_TIM_TimeBase](../01_TIM_TimeBase)와 같은 1ms 시간 기준을 **인터럽트 없이 폴링**으로 처리한다.
main 루프가 TIM3의 업데이트 플래그(`SR.UIF`)를 직접 확인·클리어하고, 1000번(=1초)마다 LD2(PA5)를 토글한다.

## 01_TIM_TimeBase(인터럽트)와의 차이

| 항목 | 01 인터럽트 방식 | 02 폴링 방식 (이 예제) |
|---|---|---|
| 타이머 시작 | `HAL_TIM_Base_Start_IT` (`DIER.UIE=1`, `CR1.CEN=1`) | `HAL_TIM_Base_Start` (`CR1.CEN=1`만) |
| NVIC `TIM3_IRQn` | 활성 | 비활성 (`.ioc`, `stm32f1xx_hal_msp.c`) |
| `TIM3_IRQHandler` (ISR) | 있음 (`stm32f1xx_it.c`) | 없음 |
| `HAL_TIM_PeriodElapsedCallback` | 있음 (ISR 문맥) | 없음 |
| `SR.UIF` 확인·클리어 | `HAL_TIM_IRQHandler()`가 처리 | main 루프가 직접 처리 |
| main 루프가 바쁠 때 | 틱을 놓치지 않는다 | 1ms 넘게 막히면 틱을 놓친다 |

PSC(63)·ARR(999)은 01과 같다. `f_update = 64MHz / ((63+1) × (999+1)) = 1kHz`

## 핵심 학습 내용

- **업데이트 플래그**: `TIM3_SR.UIF`(bit0)는 CNT가 ARR에서 0으로 넘어갈 때 하드웨어가 세운다. `DIER.UIE=0`이면 인터럽트는 나지 않고 플래그만 선다.
- **rc_w0 비트**: UIF는 0을 써서 지운다. `__HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE)`는 `SR = ~UIF`를 쓴다.
- **시작 전 클리어**: `HAL_TIM_Base_Init()`의 `EGR.UG`로 UIF가 남을 수 있으므로 `HAL_TIM_Base_Start()` 직전에 지운다 (HAL 버전에 따라 Init이 지워 주기도 한다).
- **먼저 지우고 일한다**: UIF를 확인하자마자 클리어한 뒤 작업한다. 작업 후에 지우면 작업 중 새로 선 UIF까지 지워 버린다.
- **폴링의 한계**: UIF는 1비트라 횟수를 쌓지 못한다. 루프 한 바퀴가 주기(1ms)를 넘으면 그 사이 여러 번의 오버플로가 1번으로 합쳐진다.

## 동작

LD2가 약 1초 주기로 토글되고, USART2(ST-LINK 가상 COM, 115200 8N1)로 매초 다음을 출력한다.

```
TIM3 TimeBase start (1ms update, UIF polling, print every 1s)
[1 s] tick=1000 ms lag=0 ms
[2 s] tick=2003 ms lag=3 ms
[3 s] tick=3005 ms lag=5 ms
...
```

(lag 값은 예시이다. 메시지 길이에 따라 증가 폭이 달라진다.)

- `tick`: SysTick 인터럽트로 세는 `HAL_GetTick()` 경과 시간 — 놓치지 않는 기준 시계
- `lag`: `tick - 초×1000` — 폴링으로 놓친 1ms 틱의 누적

printf 한 줄(`\r\n` 포함 약 29바이트)은 115200bps에서 약 2.5ms가 걸린다. 그동안 UIF가 2~3번 서지만 1번으로만 보이므로 **lag가 매초 1~2ms씩 커진다.**
같은 printf를 쓰는 01(인터럽트 방식)은 printf 도중에도 ISR이 틱을 세므로 이런 누적이 생기지 않는다.

## 생각해 볼 것

- printf를 지우면 lag가 0으로 유지되는지 확인한다.
- 놓치지 않으려면: 인터럽트 방식(01), 하드웨어 주기 자체를 1초로 늘리기(PSC=6399, ARR=9999), UART를 인터럽트/DMA 송신으로 바꿔 루프를 막지 않기.
- 폴링이 알맞은 경우: 루프가 짧고 단순할 때, 인터럽트를 쓸 수 없거나 짧은 구간을 바쁜 대기로 재야 할 때.

## 관련 예제

인터럽트 방식 `01_TIM_TimeBase`, 다음 단계 `03_TIM_Overflow_Counter`(오버플로를 세어 µs 시계로 확장, printf 시간 실측), 비차단 상태 기계 `08_Clock_System/11_NonBlocking_StateMachine_c`, SysTick `08_Clock_System/05_SysTick_Reg_c`.

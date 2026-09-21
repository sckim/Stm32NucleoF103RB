# 72_NVIC_Priority_HAL_c

> 계층: **HAL** + NVIC — Claude 작성, 검토 전 (컴파일만 확인)

NVIC 우선순위 그룹(선점/서브)과 인터럽트 중첩을 실험으로 확인한다.

## 핵심 학습 내용

- **실험 구성**: TIM3 ISR(10ms 주기, 짧음) = "급한 인터럽트", TIM2 ISR(100ms 주기, 일부러 20ms 바쁜 대기) = "오래 걸리는 인터럽트". TIM2 ISR 실행 20ms 동안 TIM3가 제때 들어올 수 있는지가 우선순위 설정에 달렸다. TIM3 진입 간격의 **최대값(지터)** 을 측정한다.
- **B1을 누를 때마다 케이스 A → B → C** 로 전환, 1초마다 결과를 UART(115200) 출력.
  - **A** (TIM3 선점 0 < TIM2 1): TIM3가 TIM2 ISR을 **선점(중첩)** → 최대 간격 ~10ms 유지
  - **B** (선점 우선순위 같음, 서브만 TIM3가 높음): 서브 우선순위는 **선점하지 못한다** → 최대 간격 ~20~30ms
  - **C** (TIM3 선점 2, TIM2 선점 1): 긴 ISR에 더 높은 우선순위를 준 잘못된 배정 → B와 비슷하게 지연
- **교훈**: "짧고 급한" 인터럽트에 높은 우선순위를, "길고 덜 급한" 인터럽트에 낮은 우선순위를 준다.
- **NVIC 우선순위 구조** (F103은 4비트만 구현): `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2)` → `SCB->AIRCR.PRIGROUP = 5`, 우선순위 필드 [7:4] 중 상위 2비트 = 선점(0~3), 하위 2비트 = 서브(0~3). `HAL_NVIC_SetPriority(irq, preempt, sub)`가 `NVIC->IP[irq]`에 기록. **숫자가 작을수록 우선순위가 높다.** 같은 선점 우선순위끼리는 중첩 불가.
- SysTick은 `TICK_INT_PRIORITY=0`(최고)이라 `HAL_Delay`가 흔들리지 않는다.
- 측정은 DWT 사이클 카운터(64MHz).

## ISR vs 콜백

`TIM2_IRQHandler` / `TIM3_IRQHandler`(진짜 ISR) → `HAL_TIM_IRQHandler()` → `HAL_TIM_PeriodElapsedCallback()`(이 파일): 두 타이머가 같은 콜백을 공유하므로 `htim->Instance`로 구분. ISR 안에서 UART 출력은 하지 않고 통계만 갱신, 출력은 main.

## 관련 예제

`06_Timers_RTC/50_TIM_TimeBase`(타이머 인터럽트 기초), `75_PendSV_ContextSwitch_Reg_c`(PendSV를 최저 우선순위로 두는 이유).

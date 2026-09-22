# 01_TIM_TimeBase

> 계층: **HAL** + TIM (STM32CubeIDE/CubeMX 프로젝트: `Src`, `Inc`, `.ioc`)

TIM3을 **1ms 주기 인터럽트**의 시간 기준으로 쓰고, 1000번(=1초)마다 LD2(PA5)를 토글한다.

## 핵심 학습 내용

- **타이머 주기 계산**: TIM3 클럭 64MHz, `Prescaler = 63` → 카운터 클럭 64MHz/64 = 1MHz(1µs), `Period(ARR) = 999` → 1000카운트 = **1ms(1kHz)**.
  `f_update = 64MHz / ((63+1) × (999+1)) = 1kHz`
- **자동 재로드 프리로드**: `AutoReloadPreload = ENABLE`이면 ARR 변경이 다음 업데이트 시점에 안전하게 반영된다.
- **타이머 인터럽트 시작**: `HAL_TIM_Base_Start_IT(&htim3)`가 `TIM3->DIER.UIE`와 `CR1.CEN`을 켠다.
- **소프트웨어 분주**: 콜백에서 `gTimerCnt`를 세어 1000이 되면 토글 → 하드웨어 타이머 한 개로 더 긴 주기를 만드는 방법.
- **ISR vs 콜백**: `TIM3_IRQHandler`(`stm32f1xx_it.c`, 진짜 ISR) → `HAL_TIM_IRQHandler()`가 `SR.UIF`를 지우고 → `HAL_TIM_PeriodElapsedCallback()`(이 예제 `main.c`)을 호출. 콜백은 ISR 문맥이므로 짧게 끝낸다.
- `HAL_Delay()` 없이 정확한 주기 동작을 만드는 기본기 (블로킹 대기와의 대비).

## 동작

LD2가 1초 주기로 토글된다 (1ms 인터럽트 × 1000).

## 관련 예제

PWM `02_TIM_PWM_HAL_c`, 입력 캡처 `03_TIM_InputCapture_HAL_c`. 인터럽트 우선순위 `08_Clock_System/06_NVIC_Priority_HAL_c`.

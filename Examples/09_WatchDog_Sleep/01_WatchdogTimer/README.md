# 01_WatchdogTimer

> 계층: **HAL** + **WWDG(윈도우 워치독)** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

WWDG를 초기화하고 main 루프에서 주기적으로 `HAL_WWDG_Refresh()`를 호출하는 워치독의 기본 사용법.

## 핵심 학습 내용

- **워치독 패턴**: 초기화 → 정상 동작 중 주기적 리프레시(킥) → 프로그램이 멈춰 리프레시를 못 하면 하드웨어가 리셋.
- **WWDG 설정 (`MX_WWDG_Init`)**: `Prescaler = WWDG_PRESCALER_8`, `Window = 64`, `Counter = 64`, `EWIMode = DISABLE`. 7비트 다운카운터가 `0x3F` 아래로 내려가면 리셋되며, 카운터가 윈도우 값보다 클 때 리프레시해도 리셋된다.
- 루프는 `HAL_Delay(50)` 후 `HAL_WWDG_Refresh(&hwwdg)`를 호출하고, 20회(=약 1초)마다 LD2를 토글한다.

## 확인해 볼 점 (코드 기준 계산)

WWDG 카운터 틱은 `4096 × 8 / PCLK1(32MHz) ≈ 1.024ms`이다. `Counter = 64(0x40)`에서 시작하면 리셋 임계(0x3F)까지 약 1틱(≈1ms)뿐인데 루프의 리프레시 주기는 50ms이므로, **이 설정에서는 리프레시 전에 리셋이 반복될 가능성이 크다.** 정상 동작(LD2 점멸)하지 않는다면 `Counter`를 `0x7F` 쪽으로 늘리고 `Window`를 그보다 작게 잡는 식으로 타이밍을 다시 계산해 보자. 계산 방법과 동작하는 예는 `02_WWDG_HAL_c` 참고.

## 참고: IWDG와의 차이

이 예제는 이름과 달리 **독립 워치독(IWDG)이 아니라 WWDG**를 사용한다. IWDG는 저속 내부 클럭(LSI 약 40kHz)으로 동작하고 타임아웃 전에만 리프레시하면 되는 반면, WWDG는 APB1 클럭 기반이며 "너무 이른" 리프레시도 잡아낸다.

## 관련 예제

`02_WWDG_HAL_c`(윈도우 타이밍 계산과 조기 경고), `08_Clock_System/10_DeviceInfo_UniqueID_Reg_c`(리셋 원인 확인).

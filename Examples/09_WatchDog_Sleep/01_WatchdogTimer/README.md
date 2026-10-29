# 01_WatchdogTimer

> 계층: **HAL** + **WWDG(윈도우 워치독)** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

WWDG를 초기화하고 main 루프에서 주기적으로 `HAL_WWDG_Refresh()`를 호출하는 워치독의 기본 사용법.

## 핵심 학습 내용

- **워치독 패턴**: 초기화 → 정상 동작 중 주기적 리프레시(킥) → 프로그램이 멈춰 리프레시를 못 하면 하드웨어가 리셋.
- **WWDG 설정 (`MX_WWDG_Init`)**: `Prescaler = WWDG_PRESCALER_8`, `Window = 96(0x60)`, `Counter = 127(0x7F)`, `EWIMode = DISABLE`. 7비트 다운카운터가 `0x3F` 아래로 내려가면 리셋되며, 카운터가 윈도우 값보다 클 때 리프레시해도 리셋된다.
- 루프는 `HAL_Delay(50)` 후 `HAL_WWDG_Refresh(&hwwdg)`를 호출하고, 20회(=약 1초)마다 LD2를 토글한다.
- 부팅 시 `RCC_CSR.WWDGRSTF`를 읽어 리셋 원인을 USART2(115200bps)로 출력한다 (`Reset by WWDG` / `Power-on / NRST reset`).

## 타이밍 계산

WWDG 카운터 틱은 `4096 × 8 / PCLK1(32MHz) ≈ 1.024ms`이다.

| 구분 | 계산 | 시간 |
|---|---|---|
| 이른 한계 (윈도우) | 0x7F → 0x60, 31틱 | ≈ 31.7ms |
| 늦은 한계 (타임아웃) | 0x7F → 0x3F, 64틱 | ≈ 65.5ms |

리프레시 허용 구간은 약 32~65ms이고, 루프 주기 50ms는 그 안에 있으므로 정상 동작한다.

## 동작 확인

- **정상**: LD2가 1초마다 토글되고, 시리얼 터미널에는 전원 투입 시 `Power-on / NRST reset`이 한 번만 출력된다.
- **늦은 리프레시**: `HAL_Delay(50)` → `HAL_Delay(100)` → `Reset by WWDG`가 반복 출력되고 LD2는 켜지지 않는다.
- **이른 리프레시**: `HAL_Delay(50)` → `HAL_Delay(10)` → 윈도우 위반으로 같은 리셋 루프가 생긴다 (WWDG만의 특성).
- **디버깅**: 브레이크포인트에서 멈춰도 WWDG는 계속 카운트하므로, 단계 실행이 필요하면 `__HAL_DBGMCU_FREEZE_WWDG()`(`DBGMCU_CR.DBG_WWDG_STOP`)를 호출한다.

## 참고: IWDG와의 차이

이 예제는 이름과 달리 **독립 워치독(IWDG)이 아니라 WWDG**를 사용한다. IWDG는 저속 내부 클럭(LSI 약 40kHz)으로 동작하고 타임아웃 전에만 리프레시하면 되는 반면, WWDG는 APB1 클럭 기반이며 "너무 이른" 리프레시도 잡아낸다.

## 관련 예제

`02_WWDG_c`(윈도우 타이밍 계산과 조기 경고), `08_Clock_System/12_DeviceInfo_UniqueID_Reg_c`(리셋 원인 확인).

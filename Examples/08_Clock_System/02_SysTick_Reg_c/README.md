# 02_SysTick_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — Claude 작성, 검토 전 (컴파일만 확인)

SysTick으로 1ms 틱을 만들고 `millis()` 방식의 **논블로킹 타이밍**으로 LED 점멸과 버튼 처리를 동시에 수행한다.

## 핵심 학습 내용

- **SysTick 레지스터** (Cortex-M3, PM0056 4.5): `LOAD`(주기 = (LOAD+1)/클럭, 8MHz에서 1ms → 7999), `VAL`(현재값, 쓰면 0), `CTRL`(bit2 CLKSOURCE, bit1 TICKINT, bit0 ENABLE).
- **논블로킹 설계**: `while` 안에 블로킹 delay 없이 `g_ms`(SysTick ISR이 증가)와 마지막 시각을 비교해 각 작업의 주기를 관리. 태스크 A는 LD2를 `blink_ms` 주기로 토글, 태스크 B는 20ms마다 B1을 읽어 **2회 연속 눌림**이면 `blink_ms`를 500 ↔ 100으로 전환(간이 디바운스).
- 두 작업이 서로를 막지 않고 각자의 주기로 동작함을 확인 (`04_BlinkReg`의 블로킹 딜레이와 대비).
- 리셋 기본 클럭은 HSI 8MHz.

## ISR

`SysTick_Handler`(벡터 테이블의 weak 심볼 재정의). 콜백 개념은 없다.

## 관련 예제

`01_GPIO/04_BlinkReg`(블로킹 딜레이), `09_NonBlocking_StateMachine_HAL_c`(같은 사상의 상태 머신 확장).

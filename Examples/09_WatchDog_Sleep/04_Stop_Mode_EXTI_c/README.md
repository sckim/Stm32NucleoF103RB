# 04_Stop_Mode_EXTI_c

> 계층: **HAL** + PWR + EXTI — 검토 전 (컴파일만 확인)

> CubeMX(`Stop_Mode_EXTI.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

**Stop 모드**에 진입하고 버튼(EXTI)으로 깨운다. SRAM과 레지스터 값이 유지되므로 깨어난 뒤 이어서 실행된다.

## 핵심 학습 내용

- **진입**: `PWR->CR.PDDS=0`, `SCB->SCR.SLEEPDEEP=1` 후 `WFI` (`HAL_PWR_EnterSTOPMode()`가 수행). `PWR->CR.LPDS=1`이면 전압 레귤레이터를 저전력으로(전류 더 감소, 웨이크업 약간 느림). 전류는 DS5319 Table 16 기준 전형값 약 24 µA(레귤레이터 Run), 약 14 µA(레귤레이터 저전력, LPDS = 1)이다(VDD 3.3 V, 25 °C).
- **웨이크업 원인**: 임의의 EXTI 라인(0~15), RTC 알람(EXTI17), USB 웨이크업 등.
- **함정 1 — 클럭 복구**: 깨어난 직후 시스템 클럭은 **HSI 8MHz로 되돌아가 있다** → `SystemClock_Config()`를 다시 호출해야 PLL 64MHz가 복구된다.
- **함정 2 — SysTick**: 원래 Stop 모드에서는 코어 클럭이 멈추므로 SysTick도 정지하고, Stop을 깨우는 것은 EXTI 라인뿐이다(RM0008 Table 14). 그러나 디버그용 `DBG_STOP=1` 상태에서는 SysTick 인터럽트가 켜져 있으면 Stop에서 깨어난다(ES096 2.2.3). 이 예제는 `DBG_STOP=1`을 설정하므로 디버거로 실행할 때 이 현상이 생긴다. 그래서 진입 전에 `HAL_SuspendTick()`으로 SysTick 인터럽트를 끄고, 깨어난 뒤 `HAL_ResumeTick()`으로 되살린다. 정오표가 권하는 회피책이 바로 이것이다.
- **디버깅 주의**: Stop 중 디버거 유지는 `DBGMCU_CR.DBG_STOP=1`. 전류 측정 시에는 디버거를 뽑고 잰다.

## 동작

1. LD2를 켜고 3초간 "실행 중" 메시지 출력
2. LD2를 끄고 Stop 진입
3. B1을 누르면 EXTI13 하강 에지로 깨어남 → 클럭 재설정 → "wake" 메시지 → 1로 돌아가 반복

## ISR vs 콜백

`EXTI15_10_IRQHandler`(진짜 ISR) → `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13)` → `HAL_GPIO_EXTI_Callback()`(이 파일): 웨이크업 원인 표시용 플래그만 설정.

## 관련 예제

`03_Sleep_Mode_c`(Sleep), `05_Standby_RTC_Wakeup_c`(Standby), `01_GPIO/06_ExtInt`(EXTI 기초).

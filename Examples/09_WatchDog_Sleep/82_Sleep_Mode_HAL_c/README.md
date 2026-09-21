# 82_Sleep_Mode_HAL_c

> 계층: **HAL + CMSIS** — Claude 작성, 검토 전 (컴파일만 확인)

**Sleep 모드(WFI)**: 할 일이 없으면 CPU 클럭을 정지한다. 바쁜 대기와 main 루프 횟수를 비교해 효과를 확인한다.

## 핵심 학습 내용

- **`WFI`(Wait For Interrupt)**: 코어 클럭(HCLK) 정지, 주변장치와 SRAM은 계속 동작. `SCB->SCR.SLEEPDEEP=0`이 Sleep(1이면 Stop/Standby). `SCB->SCR.SLEEPONEXIT`=1이면 ISR이 끝나자마자 다시 잠듦(여기서는 0).
- **깨우는 원인**: 우선순위가 허용하는 모든 인터럽트 (여기서는 SysTick 1ms, EXTI 버튼).
- **웨이크업 지연이 가장 짧다**(수 사이클) → 인터럽트 응답이 중요한 저전력 대기에 적합.
- **정량화 방법**: "1초 동안 main 루프가 몇 바퀴 돌았는가"
  - 바쁜 대기: 수십만~수백만 바퀴 (CPU가 계속 일함)
  - 슬립: 약 1000바퀴 (SysTick 1ms마다 깰 때 1바퀴) → 나머지 시간 CPU 클럭 정지
- **디버깅 주의**: Sleep 중에도 디버거가 붙어 있으려면 `DBGMCU_CR.DBG_SLEEP=1`(코드에서 설정).

## 동작

B1(PC13)을 누를 때마다 "바쁜 대기" ↔ "WFI 슬립" 모드가 바뀐다 (LD2는 두 모드 모두 0.5초 점멸). 1초마다 루프 횟수를 UART(115200)로 출력. 실제 전류 차이는 Nucleo의 IDD 점퍼(JP6)에 전류계를 연결해 확인 (64MHz 동작 vs Sleep).

## ISR vs 콜백

`SysTick_Handler`(`stm32f1xx_it.c`)가 `HAL_IncTick()`만 호출. 버튼은 폴링으로 읽는다.

## 관련 예제

`83`(Stop), `84`(Standby).

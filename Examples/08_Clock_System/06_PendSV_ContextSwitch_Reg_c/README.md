# 06_PendSV_ContextSwitch_Reg_c

> 계층: **레지스터(CMSIS) + 어셈블리**, HAL 미사용 — Claude 작성, 검토 전 (컴파일만 확인)

FreeRTOS 같은 RTOS가 "여러 작업을 동시에 실행하는 것처럼" 보이게 하는 원리를 **약 150줄**로 구현한 미니 선점형 RTOS. 원리 학습용이며 실제 제품에는 검증된 RTOS를 쓸 것.

## 핵심 학습 내용

- **Cortex-M3 스택 두 개**: `MSP`(예외/ISR/커널) vs `PSP`(작업). `EXC_RETURN = 0xFFFFFFFD` → 스레드 모드 + PSP로 복귀.
- **자동 저장 + 수동 저장**: 예외 진입 시 하드웨어가 R0-R3, R12, LR, PC, xPSR을 push/pop하므로, 소프트웨어는 **R4-R11만** 추가로 저장/복원하면 작업 전체 문맥이 완성된다.
- **동작 순서**
  1. 각 작업 스택에 "예외에서 막 복귀하려는 것처럼 보이는" 가짜 프레임을 만든다 (`stack_init`).
  2. `svc 0` → `SVC_Handler`: 첫 작업의 프레임을 PSP로 복원하고 EXC_RETURN으로 스레드 모드 진입.
  3. SysTick(1ms)이 tick을 세고 10ms마다 `SCB->ICSR.PENDSVSET`(bit28)으로 PendSV를 **보류(pend)**.
  4. PendSV 핸들러: 현재 작업 R4-R11 저장 → 다음 작업 선택 → 복원 → 복귀.
- **PendSV를 최하위 우선순위로 두는 이유**(`SCB->SHP[10] = 0xF0`): 다른 인터럽트를 방해하지 않고 **모든 ISR이 끝난 뒤** 문맥 교환을 하기 위해.

## 구성

- 작업 A: LD2(PA5)를 500ms마다 토글
- 작업 B: 1초마다 UART(115200)로 `task B, tick=..., switches=...` 출력
- 두 작업은 각자 스택을 가지고 10ms마다 강제로 교대(시분할, 라운드로빈). 작업은 delay 중 바쁜 대기만 한다.

## ISR

`SVC_Handler` / `PendSV_Handler` / `SysTick_Handler`. 콜백 개념은 없다.

## 관련 예제

`03_NVIC_Priority_HAL_c`(우선순위), `05_HardFault_Diagnosis_Reg_c`(스택 프레임).

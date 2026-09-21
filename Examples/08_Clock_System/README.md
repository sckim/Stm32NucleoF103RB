# 08_Clock_System — 클럭, 코어 시스템, 진단

클럭 트리와 Cortex-M3 코어 자원(SysTick, NVIC, SCB, DWT, PendSV)을 다룬다. 레지스터/어셈블리 수준의 예제가 많아 "MCU 내부가 어떻게 돌아가는가"를 배우는 심화 그룹이다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 70 | [70_Clock_Config_Reg_c](70_Clock_Config_Reg_c/README.md) | 레지스터 | HSI ↔ PLL 48MHz 전환 순서 |
| 71 | [71_SysTick_Reg_c](71_SysTick_Reg_c/README.md) | 레지스터 | 1ms 틱, `millis()`, 논블로킹 |
| 72 | [72_NVIC_Priority_HAL_c](72_NVIC_Priority_HAL_c/README.md) | HAL, NVIC | 선점/서브 우선순위, 인터럽트 중첩 |
| 73 | [73_AFIO_Remap_Reg_c](73_AFIO_Remap_Reg_c/README.md) | 레지스터 | JTAG 핀 해방, 타이머 핀 리맵 |
| 74 | [74_HardFault_Diagnosis_Reg_c](74_HardFault_Diagnosis_Reg_c/README.md) | 레지스터+ASM | 폴트 원인 진단(CFSR, 스택 프레임) |
| 75 | [75_PendSV_ContextSwitch_Reg_c](75_PendSV_ContextSwitch_Reg_c/README.md) | 레지스터+ASM | 미니 선점형 RTOS |
| 76 | [76_MCO_ClockOut_HAL_c](76_MCO_ClockOut_HAL_c/README.md) | HAL | 클럭을 핀으로 출력 |
| 77 | [77_DWT_Profiling_Reg_c](77_DWT_Profiling_Reg_c/README.md) | 레지스터 | 사이클 단위 실행 시간 측정 |
| 78 | [78_NonBlocking_StateMachine_HAL_c](78_NonBlocking_StateMachine_HAL_c/README.md) | HAL | 상태 머신, 디바운스 |
| 79 | [79_DeviceInfo_UniqueID_Reg_c](79_DeviceInfo_UniqueID_Reg_c/README.md) | 레지스터 | 고유 ID, 리셋 원인 |

## 이 그룹에서 배우는 것

- **클럭 트리**: HSI/HSE → PLL → SYSCLK → AHB/APB, 클럭을 올릴 때 Flash 웨이트 스테이트를 먼저 늘려야 하는 이유
- **예외/인터럽트 모델**: 벡터 테이블, 우선순위(선점/서브), SysTick, SVC, PendSV, 폴트
- **디버깅 기법**: 폴트 진단, DWT 사이클 측정, MCO 스코프 검증
- **F1 특유의 함정**: JTAG 핀 점유(AFIO), 32비트 카운터 RTC

## 메모

이 번호 대역(70~79)은 가득 찼다. 시스템/디버그 계열 예제를 더 추가하려면 새 그룹을 만든다.

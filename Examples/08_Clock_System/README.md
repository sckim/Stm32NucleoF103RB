# 08_Clock_System — 클럭, 코어 시스템, 진단

클럭 트리와 Cortex-M3 코어 자원(SysTick, NVIC, SCB, DWT, PendSV)을 다룬다. 레지스터/어셈블리 수준의 예제가 많아 "MCU 내부가 어떻게 돌아가는가"를 배우는 심화 그룹이다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_CoreRegisters_Instructions_Reg_c](01_CoreRegisters_Instructions_Reg_c) | 레지스터+ASM | 레지스터 파일, 조건 플래그, IT 조건부실행, 스택 PUSH/POP, 하드웨어 나눗셈 |
| 02 | [02_MemoryMap_Reg_c](02_MemoryMap_Reg_c) | 레지스터 | 4GB 주소 공간, 버스 계층, memory-mapped I/O, 리틀 엔디안 |
| 03 | [03_ExceptionModel_VectorTable_Reg_c](03_ExceptionModel_VectorTable_Reg_c) | 레지스터+ASM | 벡터 테이블, 예외 자동 스택 저장, EXC_RETURN, SVC |
| 04 | [04_Clock_Config_Reg_c](04_Clock_Config_Reg_c) | 레지스터 | HSI ↔ PLL 48MHz 전환 순서 |
| 05 | [05_SysTick_Reg_c](05_SysTick_Reg_c) | 레지스터 | 1ms 틱, `millis()`, 논블로킹 |
| 06 | [06_NVIC_Priority_c](06_NVIC_Priority_c) | HAL, NVIC | 선점/서브 우선순위, 인터럽트 중첩 |
| 07 | [07_AFIO_Remap_Reg_c](07_AFIO_Remap_Reg_c) | 레지스터 | JTAG 핀 해방, 타이머 핀 리맵 |
| 08 | [08_HardFault_Diagnosis_Reg_c](08_HardFault_Diagnosis_Reg_c) | 레지스터+ASM | 폴트 원인 진단(CFSR, 스택 프레임) |
| 09 | [09_MCO_ClockOut_c](09_MCO_ClockOut_c) | HAL | 클럭을 핀으로 출력 |
| 10 | [10_DWT_Profiling_Reg_c](10_DWT_Profiling_Reg_c) | 레지스터 | 사이클 단위 실행 시간 측정 |
| 11 | [11_NonBlocking_StateMachine_c](11_NonBlocking_StateMachine_c) | HAL | 상태 머신, 디바운스 |
| 12 | [12_DeviceInfo_UniqueID_Reg_c](12_DeviceInfo_UniqueID_Reg_c) | 레지스터 | 고유 ID, 리셋 원인 |
| 13 | [13_BusAccess_Latency_Reg_c](13_BusAccess_Latency_Reg_c) | 레지스터 | 버스별 접근 사이클 실측(SRAM·Flash·APB2·APB1), APB 8/16비트 복제, 비트 밴딩 별칭 — 02 다음에 본다 |
| 14 | [14_Clock_Gating_FlashWait_Reg_c](14_Clock_Gating_FlashWait_Reg_c) | 레지스터 | 클럭 게이팅·리셋 레지스터 실측, Flash 웨이트·프리페치별 실행 사이클 비교 — 04 다음에 본다 |
| 15 | [15_NVIC_Pending_TailChain_Reg_c](15_NVIC_Pending_TailChain_Reg_c) | 레지스터 | NVIC 대기·우선순위 판정·중첩·테일 체이닝·BASEPRI를 순서 기록과 사이클로 확인 — 06 다음에 본다 |
| 16 | [16_EXTI_Shared_PR_Reg_c](16_EXTI_Shared_PR_Reg_c) | 레지스터 | EXTI 경로 직접 설정, 공유 벡터 판별, PR 지우기 시점과 재진입, 접점 튐 — 15 다음에 본다 |

13~16은 강의자료 폴더의 교안 「메모리 맵과 버스」(13), 「클럭 트리(RCC)」(14), 「인터럽트 체계(NVIC·EXTI·SysTick)」(15·16)에 맞춰 추가한 레지스터 수준 보충 예제이다.

## 이 그룹에서 배우는 것

- **클럭 트리**: HSI/HSE → PLL → SYSCLK → AHB/APB, 클럭을 올릴 때 Flash 웨이트 스테이트를 먼저 늘려야 하는 이유
- **예외/인터럽트 모델**: 벡터 테이블, 우선순위(선점/서브), SysTick, SVC, PendSV, 폴트
- **디버깅 기법**: 폴트 진단, DWT 사이클 측정, MCO 스코프 검증
- **F1 특유의 함정**: JTAG 핀 점유(AFIO), 32비트 카운터 RTC

## 메모

폴더 번호는 이 그룹 안에서 01부터 순서대로 붙인다. 예제를 더 추가하면 다음 번호를 쓴다.

# 01_CoreRegisters_Instructions_Reg_c

> 계층: **레지스터(CMSIS) + 어셈블리** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

이 그룹의 출발점. 지금까지는 "주변장치를 어떻게 쓰는가"만 다뤘다. 이 예제는 그 아래에서 명령을 실제로 실행하는 **Cortex-M3 코어 자체**(레지스터 파일, 조건 플래그, 스택)를 들여다본다.

## 핵심 학습 내용

- **레지스터 파일**(PM0056 2.1.3): R0~R12 범용, R13(SP, MSP/PSP 두 벌), R14(LR), R15(PC), 특수 레지스터 xPSR/CONTROL/PRIMASK. `__get_MSP()` 등 CMSIS 함수로 C 에서 직접 읽는다.
- **조건 플래그 N Z C V**(3.5.3): `ADDS`/`SUBS`처럼 S 가 붙은 명령만 APSR 을 갱신한다. 오버플로·빌림을 인라인 어셈블리로 직접 관찰.
- **조건부 실행 IT**(3.8.7): Thumb-2 특유 기능(Cortex-M0 에는 없음). `ite gt` + `movgt`/`movle` 두 줄로 분기 없는 if/else 를 만든다.
- **PUSH/POP 과 스택**(3.4.6, 3.4.7): 함수를 재귀 호출하며 SP 가 깊이마다 어떻게 내려가는지 직접 찍는다.
- **Thumb 상태 비트**: 함수 포인터의 LSB 가 항상 1 인 이유 (M3 는 Thumb 전용, BX/BLX 가 이 비트로 판별).
- **하드웨어 나눗셈 SDIV**(3.6.3): "Single-cycle multiplication and hardware division"(데이터시트)의 division 쪽을 명령 하나로 확인.

## 배선 / 동작

배선 없음. UART(115200)에 위 6개 항목을 순서대로 출력.

## ISR

사용하지 않는다.

## 관련 예제

`02_MemoryMap_Reg_c`, `03_ExceptionModel_VectorTable_Reg_c`, `08_Clock_System/10_DWT_Profiling_Reg_c`(정확한 사이클 측정).

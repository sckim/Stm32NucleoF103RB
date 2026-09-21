# 05_HardFault_Diagnosis_Reg_c

> 계층: **레지스터(CMSIS) + 어셈블리**, HAL 미사용 — Claude 작성, 검토 전 (컴파일만 확인)

일부러 폴트를 일으키고, 폴트 핸들러가 "**어디서(PC) 왜(CFSR)** 죽었는지"를 UART로 출력한다. USART2를 레지스터로 직접 구동한다 (HSI 8MHz).

## 핵심 학습 내용

- **세부 폴트 핸들러 활성화**: `SCB->SHCSR`의 `USGFAULTENA(18)`, `BUSFAULTENA(17)`, `MEMFAULTENA(16)`. 켜지 않으면 모든 폴트가 HardFault로 격상되어 원인 구분이 어렵다 (`SCB->HFSR.FORCED(30)`).
- **폴트 상태 레지스터** (PM0056 4.3): `SCB->CFSR`(`[7:0]` MMFSR, `[15:8]` BFSR, `[31:16]` UFSR, 1을 써서 지움), `SCB->BFAR/MMFAR`(폴트 주소, `BFARVALID/MMARVALID`일 때만 유효), `SCB->CCR`(`DIV_0_TRP(4)`, `UNALIGN_TRP(3)`).
- **스택 프레임**: 예외 진입 시 하드웨어가 R0-R3, R12, LR, PC, xPSR을 자동 push한다 → **stacked PC = 폴트를 일으킨 명령어 주소**. `arm-none-eabi-addr2line`으로 소스 라인을 찾는다.
- **naked 핸들러 + 어셈블리**: `LR`(EXC_RETURN)의 bit2로 MSP/PSP 중 어느 스택에 프레임이 있는지 판별해 C 함수로 넘긴다.

## 사용법

터미널(115200 8N1)에서 숫자 키 입력:

| 키 | 유발 | 결과 |
|---|---|---|
| 1 | 0으로 나누기 | UsageFault, `UFSR.DIVBYZERO` (`CCR.DIV_0_TRP` 때문) |
| 2 | 정렬 안 된 주소 접근 | UsageFault, `UFSR.UNALIGNED` (`CCR.UNALIGN_TRP` 때문) |
| 3 | 존재하지 않는 주소 읽기 | BusFault, `BFSR.PRECISERR` + `BFAR`(폴트 주소) |
| 4 | 짝수 주소로 분기(Thumb 비트 0) | UsageFault, `UFSR.INVSTATE` |

폴트가 나면 정보를 출력하고 LD2를 빠르게 깜빡이며 정지한다. 리셋 버튼으로 재시작.

## ISR

`HardFault` / `MemManage` / `BusFault` / `UsageFault_Handler`. 콜백 개념은 없다.

## 관련 예제

`02_USART/05_UART_CLI_HAL_c`(`peek`로 잘못된 주소를 읽으면 발생), `06_PendSV_ContextSwitch_Reg_c`(같은 스택 프레임 구조).

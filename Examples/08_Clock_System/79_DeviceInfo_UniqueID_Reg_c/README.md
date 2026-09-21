# 79_DeviceInfo_UniqueID_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

칩 식별 정보(96비트 고유 ID, Flash 크기, 디바이스 ID)와 **리셋 원인**을 읽어 UART로 출력한다.

## 핵심 학습 내용

| 정보 | 위치 |
|---|---|
| 96비트 고유 ID | `0x1FFFF7E8 ~ 0x1FFFF7F3` (읽기 전용, 공장 기록). 시리얼 번호/암호화 키 시드/통신 주소 생성에 활용 |
| Flash 크기(KB) | `0x1FFFF7E0` (16비트). Nucleo-F103RB는 128 |
| `DBGMCU->IDCODE` | `0xE0042000`: `DEV_ID[11:0] = 0x410`(medium-density), `REV_ID[31:16]` |
| `SCB->CPUID` | `0xE000ED00`: Cortex-M3 r1p1이면 `0x411FC231` |
| `RCC->CSR` | `0x40021024`: 리셋 원인 플래그 |

- **리셋 원인 플래그**: `LPWRRSTF(31)`, `WWDGRSTF(30)`, `IWDGRSTF(29)`, `SFTRSTF(28)`(NVIC_SystemReset), `PORRSTF(27)`(전원 인가), `PINRSTF(26)`(NRST 핀), `RMVF(24)`에 1을 쓰면 플래그 삭제.
- 링커 스크립트가 아니라 **실제 소자 기록값**으로 Flash 128KB / SRAM 20KB를 확인.
- 리셋 원인 진단은 워치독/저전력 예제(`09_WatchDog_Sleep`)에서 재사용된다.

## 동작

부팅 때 위 정보를 UART(115200, HSI 8MHz)로 한 번 출력하고 LD2 점멸. NRST 버튼(PINRST), 전원 재인가(PORRST)를 바꿔 가며 리셋 원인 표시가 바뀌는 것을 확인한다.

## 인터럽트

사용하지 않는다.

## 관련 예제

`09_WatchDog_Sleep/81_WWDG_HAL_c`, `84_Standby_RTC_Wakeup_HAL_c`(리셋 원인 판별).

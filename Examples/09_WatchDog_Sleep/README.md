# 09_WatchDog_Sleep — 워치독과 저전력 모드

프로그램이 멈췄을 때 스스로 복구하는 **워치독**과, 전력을 아끼는 **Sleep / Stop / Standby** 모드를 다룬다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 80 | [80_WatchdogTimer](80_WatchdogTimer/README.md) | HAL, WWDG | 윈도우 워치독 기본 사용 (사용자 작성) |
| 81 | [81_WWDG_HAL_c](81_WWDG_HAL_c/README.md) | HAL, WWDG | 윈도우 타이밍 설계, 조기 경고 콜백 |
| 82 | [82_Sleep_Mode_HAL_c](82_Sleep_Mode_HAL_c/README.md) | HAL, PWR | WFI Sleep, main 루프 횟수 비교 |
| 83 | [83_Stop_Mode_EXTI_HAL_c](83_Stop_Mode_EXTI_HAL_c/README.md) | HAL, PWR, EXTI | Stop 진입, 버튼 웨이크업, 클럭 복구 |
| 84 | [84_Standby_RTC_Wakeup_HAL_c](84_Standby_RTC_Wakeup_HAL_c/README.md) | HAL, PWR, RTC | Standby, RTC 알람 웨이크업, BKP 유지 |

## 저전력 모드 비교 (RM0008 5장)

| 모드 | 정지되는 것 | 유지되는 것 | 웨이크업 | 예제 |
|---|---|---|---|---|
| Sleep | CPU 클럭만 | 모든 상태 | 임의 인터럽트, 즉시 | 82 |
| Stop | 고속 클럭/PLL | SRAM/레지스터 | EXTI 등, 이어서 실행 (**클럭 재설정 필요**) | 83 |
| Standby | 1.8V 도메인 전원 | 백업 도메인(RTC, BKP)만 | WKUP 핀, RTC 알람 등, **리셋과 동일** | 84 |

## 이 그룹에서 배우는 것

- IWDG(독립, 늦는 것만 감시) vs WWDG(윈도우, 너무 이른 리프레시도 감시)
- 리셋 원인 판별(`RCC->CSR`)로 워치독 리셋과 정상 부팅을 구분
- 전력 vs 웨이크업 지연 vs 유지 상태의 트레이드오프
- 디버깅 주의: 저전력 진입 시 디버거 연결이 끊길 수 있음 (`DBGMCU_CR.DBG_SLEEP/STOP`)
- 전류는 Nucleo의 IDD 점퍼(JP6)에 전류계를 연결해 측정

## 앞으로 추가 예정

`85_BKP_Registers`, 클럭 보안 시스템(CSS).

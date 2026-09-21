# 84_Standby_RTC_Wakeup_HAL_c

> 계층: **HAL** + PWR + RTC — Claude 작성, 검토 전 (컴파일만 확인)

**Standby 모드** 진입, **RTC 알람**(10초 뒤)으로 웨이크업, **백업 레지스터(BKP)** 로 상태를 유지한다.

## 핵심 학습 내용

- **Standby의 성격**: 1.8V 도메인 전원이 차단되어 SRAM·레지스터 내용이 **소실**되고, 웨이크업 = **리셋**(`main`부터 다시 시작). 전류가 가장 낮다(약 2µA). 유지되는 것은 백업 도메인(RTC, BKP `DR1~DR10`)과 Standby 회로뿐이다. → 상태를 이어가려면 BKP에 저장해 둔다.
- **핵심 레지스터/비트**: `PWR->CR.PDDS=1` + `SCB->SCR.SLEEPDEEP=1` + `WFI`(`HAL_PWR_EnterSTANDBYMode`), **`PWR->CSR.SBF`**("Standby에서 깨어났음", `CR.CSBF`에 1을 써서 지움), `PWR->CSR.EWUP`(WKUP 핀 PA0 웨이크업 허용), `RTC->ALRH/ALRL`(알람 값), `RTC->CRH.ALRIE`, `RTC->CRL.ALRF`, `BKP->DR1~DR10`(`PWR->CR.DBP=1`로 쓰기 허용).
- **RTC 클럭 소스**: LSE(32.768kHz 크리스털)를 먼저 시도하고 없으면 LSI(약 40kHz)로 대체.
- **부팅 시 판별**: `SBF` 플래그로 "전원 인가"인지 "Standby 복귀"인지 구분해 출력.
- **주의**: RTC를 다시 초기화하더라도 **백업 도메인 리셋(BDRST)은 절대 하지 않는다** (BKP 유지).

## 동작

부팅 → (Standby 복귀인지 판별) → BKP 부팅 카운터 +1 → LD2 1초 점등 → RTC 알람 = 지금 + 10초 → Standby 진입 → 10초 뒤 리셋과 동일하게 재시작. UART(115200)에 리셋 원인(POWER-ON / STANDBY-WAKEUP)과 BKP 부팅 카운터를 출력. WKUP 핀(PA0)에 3.3V를 인가해도 깨어난다.

## 주의

Standby 중에는 SWD 디버거 연결이 끊긴다. 전류는 IDD 점퍼(JP6)로 측정.

## ISR

RTC 알람은 Standby 웨이크업(리셋) 용도라 별도 ISR/콜백이 필요 없다.

## 관련 예제

`82`(Sleep), `83`(Stop), `06_Timers_RTC/54_RTC_Calendar_HAL_c`(RTC 카운터/백업 도메인).

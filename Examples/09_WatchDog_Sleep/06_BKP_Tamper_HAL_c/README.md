# 06_BKP_Tamper_HAL_c

> 계층: **HAL(UART/클럭) + 레지스터(BKP)** — Claude 작성, 검토 전 (컴파일만 확인)

**백업 레지스터(BKP)와 탬퍼 감지**: VBAT/리셋을 넘어 값이 유지되는 `BKP_DRx`를 다루고, **TAMPER 핀(PC13 = 보드의 B1)** 에 지정된 레벨이 감지되면 하드웨어가 모든 백업 레지스터를 지우는 것을 손으로 확인한다.

## 핵심 학습 내용

- **BKP 유지 조건**(RM0008 6장): 리셋(NRST)을 눌러도 유지. VDD 와 VBAT 가 모두 꺼지면 사라진다. F103xB 는 `DR1~DR10`.
- **탬퍼**(6.3.1): `BKP->CR.TPE`=1 로 PC13 활성, `TPAL`=1 이면 Low 에서 감지. 감지 시 **모든 DRx 를 0 으로 삭제**, `CSR.TEF/TIF` 플래그, `TPIE` 인터럽트.
- **주의**: TPE=1 인 동안 PC13 은 일반 GPIO 로 못 쓴다. 버튼을 누르고 있으면 인터럽트가 폭주하므로 ISR 에서 잠시 끄고 main 이 재무장한다.
- 쓰기 전에 `PWR->CR.DBP`(백업 도메인 쓰기 허용)와 `RCC->APB1ENR.BKPEN/PWREN`.

## 배선 / 동작

B1(PC13)을 누르면 "TAMPER detected"와 지워진 DR 값을 UART(115200)로 출력. NRST 를 눌러 다시 부팅하면 DR2(부팅 횟수)가 유지되는 것과 탬퍼 뒤 초기화되는 것을 비교.

## ISR vs 콜백

`TAMPER_IRQHandler`(진짜 ISR) → `tamper_isr()`(이 파일, HAL 을 거치지 않고 `BKP->CSR.TIF` 를 직접 처리).

## 관련 예제

`05_Standby_RTC_Wakeup_HAL_c`(BKP 로 부팅 횟수 유지), `06_Timers_RTC/05_RTC_Calendar_HAL_c`.

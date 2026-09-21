# 54_RTC_Calendar_HAL_c

> 계층: **HAL**(RTC 초기화/1초 인터럽트) + **레지스터**(카운터 접근) — Claude 작성, 검토 전 (컴파일만 확인)

F1의 RTC를 이용한 달력 시계. `YYYY-MM-DD HH:MM:SS (요일)`을 1초마다 UART로 출력한다.

## 핵심 학습 내용

- **F1 RTC의 특징**: 달력 하드웨어가 없다. 32비트 카운터(`RTC->CNTH:CNTL`)가 1초마다 1 증가할 뿐이므로 이 값을 **Unix 시간**(1970-01-01부터 경과한 초)으로 쓰고 날짜/시각/요일은 소프트웨어로 계산한다. (이후 시리즈는 BCD 달력 하드웨어가 있다.)
- **카운터 읽기 경합 방지**: `CNTH → CNTL → CNTH`를 읽어 상위가 바뀌었으면 재시도(자리올림 경합).
- **쓰기 절차**: `CRL.RTOFF=1` 대기 → `CRL.CNF=1`(설정 모드) → `CNTH/CNTL` 기록 → `CNF=0` → `RTOFF=1` 대기.
- **백업 도메인**: `PWR->CR.DBP=1`로 쓰기 허용, `RCC->BDCR`(`LSEON/LSERDY`, `RTCSEL[9:8]`, `RTCEN[15]`). NRST를 눌러도 시각이 이어지고, VBAT 없이 전원을 완전히 끄면 초기화된다.
- **최초 설정 표시**: BKP `DR1`에 "설정됨"을 기록해 두어 재부팅 시 덮어쓰지 않게 한다. 최초에는 컴파일 시각(`__DATE__`/`__TIME__`)으로 설정.
- 날짜 계산(윤년, 요일)을 정수 연산으로 구현.

## 동작

- RTC 1초 인터럽트(`CRH.SECIE`, `CRL.SECF`)마다 시각을 출력하고 LD2 토글.
- B1(PC13)을 누르면 컴파일 시각으로 재설정.

## ISR vs 콜백

`RTC_IRQHandler`(진짜 ISR) → `HAL_RTCEx_RTCIRQHandler()` → `HAL_RTCEx_RTCEventCallback()`(이 파일): 1초 이벤트. 플래그만 세우고 출력은 main에서.

## 관련 예제

`09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c`(RTC 알람 + BKP).

# 06_IAP_Xmodem_Bootloader_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**UART 로 새 펌웨어를 받아 Flash 에 기록하는 부트로더**(XMODEM-CRC). ST-LINK 없이 현장에서 업데이트하는 IAP 구조이며, 앱은 `04_IAP_App_Reg_c` 를 사용한다.

## 핵심 학습 내용

- **흐름**: B1 을 누른 채 리셋(또는 유효한 앱 없음) → 'C' 를 1초마다 보내 CRC 모드 요청(최대 60초) → PC 가 `firmware.bin` 전송 → 앱 검사 후 점프.
- **XMODEM-CRC**: `[SOH|STX][블록][~블록][데이터 128|1024][CRC16 hi][CRC16 lo]`. 검사 후 ACK/NAK, EOT 로 종료, 중복 블록은 ACK 만, CAN(0x18)은 취소. CRC16-CCITT(0x1021, 초기값 0) — 표준 시험 벡터("123456789" = 0x31C3)로 확인.
- **Flash 기록(레지스터)**: `FLASH->KEYR` 잠금 해제, 페이지 첫 쓰기 직전 `CR.PER/AR/STRT` 소거, `CR.PG` 반워드 프로그램 후 읽어 검증. 앱 영역(0x08008000~0x0801FFFF) 밖으로는 쓰지 않는다.
- **안전**: 전송 도중 실패하면 벡터 테이블이 지워진 상태라 앱 검사에서 탈락해 점프하지 않는다. ms 타이밍은 SysTick `COUNTFLAG` 폴링.

## 배선 / 동작

1) 이 프로젝트를 업로드(부트로더 32KB, `ldscript.ld`) 2) `04_IAP_App_Reg_c` 를 빌드해 `firmware.bin` 생성 3) B1 을 누른 채 리셋 4) Tera Term 의 File > Transfer > XMODEM > Send(**CRC** 옵션)로 전송. UART 115200.

## ISR vs 콜백

인터럽트를 사용하지 않는다 (폴링).

## 관련 예제

`03_IAP_Bootloader_Reg_c`(점프만 하는 버전), `04_IAP_App_Reg_c`(짝), Docs AN2606(ST 시스템 부트로더).

# 04_Clock_Config_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — Claude 작성, 검토 전 (컴파일만 확인)

B1(PC13)을 누를 때마다 RCC/FLASH 레지스터로 SYSCLK를 **HSI 8MHz ↔ PLL 48MHz**로 전환한다.

## 핵심 학습 내용

- **클럭 경로** (RM0008 7장): `HSI 8MHz → /2 → PLL ×12 = 48MHz → SYSCLK → AHB(/1)=48MHz → APB1(/2)=24MHz, APB2(/1)=48MHz`.
- **전환 순서가 핵심** (틀리면 Flash 접근 오류/하드폴트)
  - 올릴 때: FLASH 웨이트 스테이트 증가 → APB1 분주 → PLL 켜고 `PLLRDY` 대기 → `SW`를 PLL로 → `SWS` 확인
  - 내릴 때: `SW`를 HSI로 → `SWS` 확인 → PLL 끄기 → FLASH 웨이트 스테이트 감소
- 관련 레지스터: `RCC->CR`(HSION/PLLON/PLLRDY), `RCC->CFGR`(SW/SWS, PLLSRC, PLLMUL, HPRE, PPRE1/2, MCO), `FLASH->ACR`(LATENCY).
- 64MHz 대신 48MHz를 쓰는 이유: MCO 최대 주파수 50MHz.

## 관찰 방법

1. LD2 점멸 속도: 바쁜 대기(for 루프) 기반이라 48MHz에서 6배 빠르게 깜빡인다.
2. MCO 출력 PA8(Arduino D7)에 오실로스코프/로직 분석기를 연결하면 SYSCLK(8MHz ↔ 48MHz)가 그대로 보인다.

## 인터럽트

사용하지 않는다 (폴링).

## 관련 예제

`09_MCO_ClockOut_HAL_c`(HAL 버전 MCO), HAL의 `SystemClock_Config`(모든 HAL 예제).

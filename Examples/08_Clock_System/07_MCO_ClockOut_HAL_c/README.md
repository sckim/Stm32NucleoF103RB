# 07_MCO_ClockOut_HAL_c

> 계층: **HAL** + RCC — Claude 작성, 검토 전 (컴파일만 확인)

**MCO(Microcontroller Clock Output)** 로 내부 클럭을 PA8(D7)로 내보내 오실로스코프로 확인한다.

## 핵심 학습 내용

- **`RCC->CFGR.MCO[26:24]`**: 000=없음, 100=SYSCLK, 101=HSI, 110=HSE, 111=PLL/2. F1의 MCO는 PLL을 항상 2분주해 내보낸다.
- **핀 설정**: PA8은 대체기능 푸시풀 50MHz(`GPIOA->CRH[3:0] = 0xB`)여야 한다. `HAL_RCC_MCOConfig`가 함께 처리한다.
- **HSE 바이패스**: Nucleo는 ST-Link의 MCO 8MHz를 HSE로 바이패스 입력한다 (`RCC->CR.HSEBYP=1, HSEON=1` → `HSERDY` 대기). 해당 납땜 브리지가 없으면 자동으로 건너뛴다.
- **최대 주파수 제약**: MCO 핀은 최대 50MHz라 SYSCLK 64MHz는 직접 내보내지 않는다.
- **용도**: 다른 칩/코덱/FPGA에 기준 클럭 공급, 크리스털/PLL 설정 검증 (시스템 클럭이 실제 몇 MHz인지 스코프로 교차 검증).

## 동작

B1(PC13)을 누를 때마다 출력 소스가 순환한다: HSI(8MHz) → PLL/2(32MHz) → HSE(8MHz). 현재 소스와 SYSCLK를 UART(115200)로 출력.

## 인터럽트

사용하지 않는다.

## 관련 예제

`01_Clock_Config_Reg_c`(레지스터 방식, 48MHz SYSCLK를 MCO로 출력).

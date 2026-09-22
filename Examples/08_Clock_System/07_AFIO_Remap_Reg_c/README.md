# 07_AFIO_Remap_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — Claude 작성, 검토 전 (컴파일만 확인)

AFIO로 **JTAG 핀을 GPIO로 해방**하고 **타이머 채널 핀을 리맵**한다. STM32F1 특유의 함정을 다룬다.

## 핵심 학습 내용

- **F1의 함정**: 리셋 직후 PA13/PA14(SWDIO/SWCLK)뿐 아니라 PA15(JTDI), PB3(JTDO), PB4(NJTRST)도 디버그 포트가 점유한다. 이 핀을 일반 GPIO/타이머 핀으로 쓰려면 `AFIO->MAPR.SWJ_CFG`를 바꿔야 한다.
- **`AFIO_MAPR` 핵심 비트** (RM0008 9.4.2)
  - `[26:24] SWJ_CFG`(쓰기 전용): 000 전체 SWJ | 001 JTAG-DP+SW-DP이지만 NJTRST 해방 | **010 SW-DP만** | 100 전부 끔
  - `[9:8] TIM2_REMAP`: 00 없음 | 01 부분1(CH1/ETR=PA15, CH2=PB3) | 10 부분2(CH3=PB10, CH4=PB11) | 11 완전
  - **`SWJ_CFG=100`(전부 끔)은 디버거가 끊겨 복구가 어렵다.** SWD는 유지(010)해야 ST-Link로 계속 다운로드/디버그 가능.
- **AFIO 클럭 선행**: `RCC->APB2ENR.AFIOEN`(bit0)을 먼저 켜야 `MAPR`에 쓸 수 있다.

## 동작

1. `SWJ_CFG=010` → PA15, PB3, PB4 해방
2. `TIM2_REMAP=11`(완전 리맵) → TIM2_CH2가 **PB3(D3)** 에 1kHz / 듀티 25% PWM 출력
3. PB4(D5)를 일반 GPIO로 200ms마다 토글 (해방 전에는 NJTRST가 점유)
4. LD2(PA5)는 0.5초 하트비트

**비교 실험**: `FREE_JTAG_PINS`를 0으로 바꾸면 (1)~(3)을 하지 않아 PB3/PB4가 JTAG로 남고 PWM/토글이 나오지 않는다.

## 인터럽트

사용하지 않는다.

## 관련 예제

`01_GPIO/03_BlinkLL`(CubeMX가 생성한 `LL_GPIO_AF_Remap_SWJ_NOJTAG`), `06_Timers_RTC/02_TIM_PWM_HAL_c`.

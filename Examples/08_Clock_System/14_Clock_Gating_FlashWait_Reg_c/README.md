# 14_Clock_Gating_FlashWait_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「클럭 트리(RCC)」 6.3절(클럭 게이팅)과 6.4절(Flash 웨이트 스테이트)의 실습이다.
"클럭을 켜야 레지스터가 응답한다"와 "클럭을 올리려면 Flash를 먼저 느리게 읽게 해야 한다"를 숫자로 확인한다.

## 핵심 학습 내용

- **실험 A — 클럭 게이팅과 리셋**: `TIM2->ARR`(리셋값 0xFFFF)를 대상으로 `RCC->APB1ENR.TIM2EN`을 껐다 켜며 쓰기·읽기를 반복하고, 마지막에 `RCC->APB1RSTR.TIM2RST` 펄스로 리셋한다.
  - 클럭이 꺼져 있으면 쓰기는 무시되고 읽으면 0이 나온다(기대).
  - 클럭을 꺼도 레지스터 내용은 지워지지 않는다. 다시 켜면 이전 값이 보인다(기대).
  - 리셋 레지스터는 내용을 리셋값으로 되돌린다.
  - "클럭이 꺼지면 읽은 값은 항상 0x0"은 RM0008 7.3.6(RCC_AHBENR)의 Note에 적혀 있다. 쓰기 무시와 내용 유지는 문서에 없는 기대이므로 실제 출력으로 확인한다.
- **실험 B — Flash 웨이트 스테이트**: `FLASH_ACR.LATENCY`는 Flash 한 번 읽는 데 기다리는 SYSCLK 수이다. 같은 함수의 실행 사이클을 `DWT->CYCCNT`로 잰다.
  - SYSCLK 8 MHz: LATENCY 0·1·2 × 프리페치 끔·켬 6가지(많이 기다리는 것은 항상 안전하다).
  - SYSCLK 64 MHz: LATENCY 2, 프리페치 켬 1가지만. 64 MHz에서 LATENCY 0·1은 금지이고, 프리페치는 SYSCLK < 24 MHz에서만 켜고 끌 수 있다(RM0008 3.3.3).
  - `bench_linear`(직선 코드)는 프리페치가 웨이트를 숨기고, `bench_branch`(분기·함수 포인터)는 웨이트가 드러난다.
- **전환 순서**: 올릴 때 웨이트 먼저 → PLL 잠김 → `SW` 전환 → `SWS` 확인. 내릴 때는 역순으로, 웨이트를 맨 나중에 낮춘다.
- **CMSIS 매크로 함정**: `FLASH_ACR_LATENCY_0/_1/_2`는 웨이트 수가 아니라 비트 위치(0x1/0x2/0x4)이다. 2 웨이트는 `FLASH_ACR_LATENCY_1`이다.

## 동작

배선 없음. 5초마다 두 실험을 반복해 USART2(ST-LINK 가상 COM, 115200 8N1)로 출력하고 LD2를 토글한다. UART BRR는 클럭을 바꿀 때마다 PCLK1에 맞춰 다시 계산한다(8 MHz → 69, 32 MHz → 278).

```
14_Clock_Gating_FlashWait_Reg_c  (reset: SYSCLK = HSI 8 MHz, FLASH_ACR = 0x0030)
[round 1]
A) clock gating on TIM2->ARR (reset value 0xFFFF)
   1) clock OFF, write 0x1234, read -> ARR = 0x0000  (as expected)
   2) clock ON,  read                -> ARR = 0xFFFF  (as expected)
   ...
B) Flash wait states (cycles measured by DWT, lower is faster)
   SYSCLK  LATENCY  prefetch |  linear  |  branch
   8 MHz      0       off  |   ....   |   ....
   ...
  64 MHz      2       on   |   ....   |   ....
```

(사이클 값은 보드에서 실측해 채운다. 최적화 수준에 따라 달라진다.)

## 생각해 볼 것

- 8 MHz에서 LATENCY를 0 → 2로 올렸을 때 `linear`와 `branch` 중 어느 쪽이 더 크게 느려지는가? 이유를 프리페치 버퍼(2 × 64비트)의 동작으로 설명해 보자.
- 64 MHz LATENCY 2의 사이클 수를 8 MHz LATENCY 0과 비교하면, 클럭은 8배인데 실제 실행 시간은 몇 배 빨라졌는가?
- `08_Clock_System/04_Clock_Config_Reg_c`는 48 MHz에서 1 웨이트를 `FLASH_ACR_LATENCY_0`으로 설정한다. 이것을 이름만 보고 `FLASH_ACR_LATENCY_1`로 바꿔 쓰면 실제로 몇 웨이트가 설정되는가? 48 MHz에서 그 설정은 허용되는가?

## 인터럽트

사용하지 않는다.

## 관련 예제

`04_Clock_Config_Reg_c`(HSI ↔ PLL 전환), `10_DWT_Profiling_Reg_c`(DWT 사이클 측정), `12_Advanced_Topics/08_HSE_PLL_72MHz_Reg_c`(72 MHz 구성).

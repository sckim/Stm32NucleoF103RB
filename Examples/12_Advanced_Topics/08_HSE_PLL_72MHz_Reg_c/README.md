# 08_HSE_PLL_72MHz_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

**HSE(바이패스) + PLL ×9 = 72MHz**(F103 최대 클럭)를 레지스터로 설정한다. 다른 예제의 HSI/2×16 = 64MHz 와 달리 이 값은 HSE 를 써야 만들 수 있다. HSE 가 없으면 HSI 로 **안전하게 복귀**한다.

## 핵심 학습 내용

- **전환 순서**: HSE 켜고 `HSERDY`(타임아웃) → Flash 웨이트 스테이트 2 + 프리페치 → APB1 /2 → PLL 설정/켜고 `PLLRDY` → `SW=PLL` → `SWS` 확인. (올릴 때 Flash 웨이트 스테이트가 **먼저**)
- **장애 처리**: HSE/PLL 이 안 올라오면 되돌려서 HSI 8MHz 로 계속 동작 ("영원히 기다리다 멈추는" 고장 방지). 동작 중 장애 감지는 CSS(`RCC_CR.CSSON`, RM0008 7.2.7).
- **보드 개정 주의**: MB1136 C-01 은 HSE 미사용, C-02 이상은 ST-LINK MCO 가 HSE 입력 (Docs/README.md 참고).
- UART BRR 를 실제 APB1 클럭에서 계산 (`BRR = pclk1/115200`).

## 배선 / 동작

MCO(PA8)에 PLL/2 = 36MHz(성공) 또는 HSI 8MHz(실패)를 출력하니 스코프로 검증할 수 있다. UART(115200)로 클럭 소스/SYSCLK/APB1 출력, LD2 는 0.5초 점멸(SysTick 기준).

## ISR vs 콜백

`SysTick_Handler`(진짜 ISR)만 사용. 콜백 개념 없음.

## 관련 예제

`08_Clock_System/04_Clock_Config_Reg_c`(HSI↔PLL 48MHz), `08_Clock_System/09_MCO_ClockOut_HAL_c`.

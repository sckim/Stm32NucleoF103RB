# 12_ITM_SWO_Printf_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

**ITM/SWO 로 printf**: UART 핀과 배선 없이 디버거 연결만으로 텍스트를 출력한다. 디버거를 떼면 출력은 버려진다.

## 핵심 학습 내용

- **구성 요소**: ITM(자극 포트 0)이 만든 바이트를 TPIU 가 직렬화해 SWO 핀(F1 은 PB3)으로 내보내고, ST-LINK 가 PC 도구(STM32CubeIDE SWV, CubeProgrammer, OpenOCD)로 전달.
- **설정 레지스터**: `CoreDebug->DEMCR.TRCENA`, `DBGMCU->CR.TRACE_IOEN`(비동기), `TPI->ACPR`(SWO 분주 = f_core/f_swo − 1), `SPPR`, `FFCR`, `ITM->LAR`(잠금 해제), `TCR`, `TER`.
- **printf 리타깃**: newlib 의 `_write()`를 재정의해 `ITM_SendChar()`로 보낸다. (`stdout` → SWO)
- **주의**: Nucleo 에서 PB3 가 ST-LINK SWO 에 연결돼 있는지 UM1724 솔더 브리지 표로 확인. 디버그 도구의 코어 클럭을 8MHz(HSI)로, SWO 를 2MHz 로 맞춘다.

## 배선 / 동작

UART 배선 불필요. 500ms 마다 tick/VTOR/SP 를 printf 로 출력하고 LD2 를 점멸. SWO 콘솔(SWV ITM Data Console 등)에서 확인.

## ISR vs 콜백

`SysTick_Handler`(진짜 ISR)만 사용. 콜백 개념 없음.

## 관련 예제

`02_USART/01_USART_printf`(UART 로 printf), `08_DWT_Profiling_Reg_c`(같은 DEMCR/DWT 블록).

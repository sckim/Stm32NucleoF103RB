# 01_ADC_Injected_HAL_c

> 계층: **HAL + DMA** — Claude 작성, 검토 전 (컴파일만 확인)

ADC **주입(injected) 채널**: 정규 변환이 연속으로 도는 중에도 급한 측정이 우선 끼어들어 실행된다. 결과는 채널별 전용 레지스터(`JDR1`~`JDR4`)에 저장되어 덮어써지지 않는다.

## 핵심 학습 내용

- **정규 vs 주입**: 정규(최대 16개, 결과가 하나의 `DR`에 쌓임) / 주입(최대 4개, 정규 변환을 중단시키고 먼저 실행, 전용 `JDRx`).
- **설정 레지스터**: `ADC1->JSQR`(시퀀스), `CR2.JEXTSEL`+`JEXTTRIG`(소프트웨어 트리거), `JOFR1`(오프셋 자동 차감), `SR.JEOC`/`CR1.JEOCIE`(완료 플래그/인터럽트).
- **응용**: 모터 전류 샘플링, 느린 센서 스캔 중 과전류 감시처럼 "정확한 타이밍의 측정"이 필요한 곳 (RM0008 11.3.9).
- 이 예제는 PA1(정규, 연속+DMA)을 계속 읽으면서 500ms 마다 내부 온도 센서를 주입 변환으로 1회 읽는다. 온도 환산은 `T = (1.43V − V)/4.3mV + 25` (개체 편차가 크다).

## 배선 / 동작

PA1(A1)에 가변저항(0~3.3V)을 연결하면 정규 채널 값이 변한다. 온도 센서는 배선이 필요 없다. UART(115200)로 두 값을 출력.

## ISR vs 콜백

`ADC1_2_IRQHandler`(진짜 ISR) → `HAL_ADC_IRQHandler()` → `HAL_ADCEx_InjectedConvCpltCallback()`(이 파일).

## 관련 예제

`03_ADC/01_ADC_Temperature`(폴링), `03_ADC/03_ADC_AnalogWatchdog_HAL_c`(연속 변환 + DMA 패턴).

# 02_ADC_DualMode_HAL_c

> 계층: **HAL + DMA** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

ADC1 과 ADC2 를 **동시에** 트리거해 두 신호를 같은 순간에 샘플링한다(듀얼 모드, 정규 동시). 한 ADC 로 채널을 번갈아 읽을 때 생기는 시간차(skew)가 없어서 전력(전압×전류)이나 위상 측정에 쓴다.

## 핵심 학습 내용

- **정규 동시 모드**(RM0008 11.9.2): `ADC->CR1.DUALMOD = 0110`. 마스터 ADC1 이 TIM3 TRGO(1kHz)로 트리거되면 슬레이브 ADC2 도 같은 순간에 변환한다.
- **32비트 결과**: `ADC1->DR` 하나에 하위 16비트 = ADC1, 상위 16비트 = ADC2 가 합쳐진다. DMA 를 32비트(WORD)로 설정해 한 번에 두 채널을 가져온다 (ADC2 는 자기 DMA 가 없다).
- **설정 요령**: ADC2 는 `ADC_SOFTWARE_START`, 두 ADC 의 샘플 시간을 같게, `HAL_ADCEx_MultiModeConfigChannel` + `HAL_ADCEx_MultiModeStart_DMA`.
- 반/전체 콜백에서 채널별 평균을 계산해 1초마다 출력한다.

## 배선 / 동작

PA0(ADC1_IN0), PA1(ADC2_IN1)에 각각 신호(가변저항 등)를 연결. 같은 신호원에 묶으면 두 값이 거의 같아야 한다. UART(115200)로 평균과 차이를 출력.

## ISR vs 콜백

`DMA1_Channel1_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → `HAL_ADC_ConvHalfCpltCallback()` / `HAL_ADC_ConvCpltCallback()`(이 파일).

## 관련 예제

`03_ADC/02_ADC_DMA_TimTrigger_HAL_c`(단일 ADC 스캔), `01_ADC_Injected_HAL_c`.

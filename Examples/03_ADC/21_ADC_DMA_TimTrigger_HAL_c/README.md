# 21_ADC_DMA_TimTrigger_HAL_c

> 계층: **HAL** + TIM + DMA — Claude 작성, 검토 전 (컴파일만 확인)

타이머가 정확한 주기(1kHz)로 ADC 변환을 시작시키고, DMA가 결과를 원형 버퍼에 저장한다. CPU는 버퍼가 반/전부 찼을 때만 개입한다.

## 핵심 학습 내용

- **왜 타이머 트리거인가**: 소프트웨어/연속 변환은 샘플 간격이 흔들리거나 변환 시간에 좌우된다. TRGO 트리거는 샘플링 주파수가 타이머로 정확히 정해져 FFT·필터 등 신호처리에 적합하다.
- **트리거 체인**: `TIM3` 업데이트 이벤트 → `TIM3->CR2.MMS=010`(TRGO) → `ADC1->CR2`(`EXTSEL=100` TIM3_TRGO, `EXTTRIG=1`) 변환 시작.
- **스캔 모드 2채널**: `CR1.SCAN=1`, `SQR1.L=1`, `SQR3={CH0, CH1}`(PA0, PA1), 샘플 55.5 사이클.
- **DMA 원형 저장**: DMA1 채널1, 주변장치→메모리, 반워드, `CIRC`. 반 버퍼(HT)와 전체(TC) 콜백으로 **더블 버퍼링**처럼 한쪽을 처리하는 동안 다른 쪽이 채워진다.
- 채널이 인터리브되어 저장되는 `adc_buf[]` 구조와 평균 계산.

## 배선

PA0(A0), PA1(A1)에 가변저항 또는 0~3.3V 신호 (미연결 시 플로팅 값).

## 동작

1kHz로 두 채널 샘플링 → HT/TC 콜백에서 평균 계산 → 1초마다 UART(115200) 출력.

## ISR vs 콜백

`DMA1_Channel1_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → `HAL_ADC_ConvHalfCpltCallback()` / `HAL_ADC_ConvCpltCallback()`(이 파일).

## 관련 예제

`20`(폴링 ADC), 타이머 기초 `06_Timers_RTC/50_TIM_TimeBase`, DMA 기초 `07_DMA/60_DMA_MemToMem_HAL_c`.

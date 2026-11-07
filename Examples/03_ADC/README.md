# 03_ADC — 아날로그 입력

STM32F103의 12비트 ADC1(약 1µs 변환, 채널 0~17)을 폴링에서 시작해 **타이머 트리거 + DMA**, 하드웨어 비교(아날로그 워치독), 기준전압 보정까지 확장한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_ADC_Single](01_ADC_Single) | HAL | 외부 핀 PA0(CH0) 한 채널, 연속 변환 + 폴링, 교정, mV 환산 |
| 02 | [02_ADC_Temperature](02_ADC_Temperature) | HAL | 내부 온도 센서(CH16) → 섭씨 환산, 긴 샘플링 시간 |
| 03 | [03_ADC_Multi](03_ADC_Multi) | HAL | 스캔 + 불연속 모드로 4채널(A0/A1/A2/Vrefint) 폴링 |
| 04 | [04_ADC_DMA_TimTrigger](04_ADC_DMA_TimTrigger) | HAL, TIM, DMA | TIM3 TRGO 1kHz 트리거, 2채널 스캔, DMA 원형 |
| 05 | [05_ADC_AnalogWatchdog_c](05_ADC_AnalogWatchdog_c) | HAL, DMA | 전압 창 이탈을 하드웨어가 감시 |
| 06 | [06_ADC_Vrefint_VDDA_c](06_ADC_Vrefint_VDDA_c) | HAL | Vrefint로 실제 VDDA 측정 후 보정 |
| 07 | [07_ADC_Potentiometer_PWM_c](07_ADC_Potentiometer_PWM_c) | HAL, DMA, TIM | 가변저항 → 이동평균/감마 → PWM 밝기 |
| 08 | [08_ADC_SampleTime_SourceZ_Reg_c](08_ADC_SampleTime_SourceZ_Reg_c) | 레지스터 | 샘플링 시간과 소스 저항: 샘플-홀드 충전 오차를 SMP별로 실측(DS 식 1) |
| 09 | [09_ADC_TriggerJitter_Reg_c](09_ADC_TriggerJitter_Reg_c) | 레지스터 | 소프트웨어 시작 vs TIM3 TRGO 트리거의 샘플링 간격 지터(DWT 실측), ADON 재기록 함정 |

08·09는 강의자료 교안 「ADC」 장에 맞춰 추가한 레지스터 수준 보충 예제이다.

## 이 그룹에서 배우는 것

- ADC 사용 순서: 클럭(ADC 최대 14MHz) → 교정 → 채널/샘플 시간 → 변환 시작
- 변환 시작 방식: 소프트웨어 / 연속 / **외부(타이머) 트리거**. 샘플 간격의 정밀도가 신호처리에 미치는 영향
- CPU 개입 최소화: DMA + Half/Full-transfer 콜백, 아날로그 워치독
- ADC 값 = "VDDA 대비 비율"이라는 점과 이를 보정하는 방법

교안(HTML): 각 예제 폴더의 `<예제명> 교안.html` (폴더명 끝의 `_c`는 파일명에서 뺀다. 예: `05_ADC_AnalogWatchdog_c/05_ADC_AnalogWatchdog 교안.html`). 03과 04는 `03_ADC_Multi/ADC 다채널 교안.html`, `04_ADC_DMA_TimTrigger/ADC 타이머 트리거 DMA 교안.html`이다.

**상위 교안**: [03_ADC 교안.html](03_ADC%20교안.html)은 ADC 하드웨어를 RM0008·DS5319의 원본 블록도로 설명한다. 다루는 내용은 샘플링·양자화·샘플-홀드·축차 비교, 클럭과 변환 시간·신호원 저항 계산, 시퀀서 모드와 트리거, 플래그·인터럽트·DMA, 설계상 한계, 확인 문제이며, 각 예제가 어느 경로를 쓰는지 연결한다. 그림 원본은 `img/` 폴더에 있다.

## 공통

ADC 클럭은 PCLK2(64MHz)를 나눠 14MHz 이하로 맞춘다. `01_ADC_Single`·`02_ADC_Temperature`는 ÷8 = 8MHz, `03`~`07`은 ÷6 = 10.67MHz이다. 입력 예제(`03_ADC_Multi`(+PA4), `04_ADC_DMA_TimTrigger`, `05_ADC_AnalogWatchdog_c`, `06_ADC_Vrefint_VDDA_c`)는 PA0/PA1에 가변저항(0~3.3V) 연결 (미연결 시 플로팅 값).

# 03_ADC — 아날로그 입력

STM32F103의 12비트 ADC1(약 1µs 변환, 채널 0~17)을 폴링에서 시작해 **타이머 트리거 + DMA**, 하드웨어 비교(아날로그 워치독), 기준전압 보정까지 확장한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 20 | [20_ADC_Temperature](20_ADC_Temperature/README.md) | HAL | 내부 온도 센서, 연속 변환, 교정 |
| 21 | [21_ADC_DMA_TimTrigger_HAL_c](21_ADC_DMA_TimTrigger_HAL_c/README.md) | HAL, TIM, DMA | TIM3 TRGO 1kHz 트리거, 2채널 스캔, DMA 원형 |
| 22 | [22_ADC_AnalogWatchdog_HAL_c](22_ADC_AnalogWatchdog_HAL_c/README.md) | HAL, DMA | 전압 창 이탈을 하드웨어가 감시 |
| 23 | [23_ADC_Vrefint_VDDA_HAL_c](23_ADC_Vrefint_VDDA_HAL_c/README.md) | HAL | Vrefint로 실제 VDDA 측정 후 보정 |

## 이 그룹에서 배우는 것

- ADC 사용 순서: 클럭(ADC 최대 14MHz) → 교정 → 채널/샘플 시간 → 변환 시작
- 변환 시작 방식: 소프트웨어 / 연속 / **외부(타이머) 트리거**. 샘플 간격의 정밀도가 신호처리에 미치는 영향
- CPU 개입 최소화: DMA + Half/Full-transfer 콜백, 아날로그 워치독
- ADC 값 = "VDDA 대비 비율"이라는 점과 이를 보정하는 방법

## 공통

ADC 클럭은 PCLK2(64MHz)/6 = 10.67MHz. 입력 예제(`21`, `22`, `23`)는 PA0/PA1에 가변저항(0~3.3V) 연결 (미연결 시 플로팅 값).

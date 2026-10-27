# 01_ADC_Single

> 계층: **HAL** (STM32CubeMX 생성 프로젝트)

ADC1의 **외부 핀 PA0(채널 0, `ADC_CHANNEL_0`)** 를 연속 변환해 원시 값과 mV 환산값을 UART로 출력한다. ADC의 가장 기본적인 사용법이다.

## 핵심 학습 내용

- **ADC 기본 사용 순서**: `HAL_ADCEx_Calibration_Start()`(교정) → `HAL_ADC_Start()` → `HAL_ADC_PollForConversion()` → `HAL_ADC_GetValue()`
- **설정 요점**: 채널 `ADC_CHANNEL_0`(PA0, ADC1_IN0), 연속 변환 모드 `ContinuousConvMode = ENABLE`, 샘플 시간 239.5 사이클.
- **ADC 클럭**: `SystemClock_Config`의 `RCC_PERIPHCLK_ADC` 분주(PCLK2/8 = 8MHz)로 ADC 클럭을 14MHz 이하로 맞춘다.
- **값의 의미**: 12비트(0~4095)는 `0V ~ VDDA` 구간의 비율이다. VDDA = 3.3V로 가정해 `mV = raw × 3300 / 4095`로 환산한다.
- `printf` 리다이렉션(`__io_putchar`)으로 결과 출력 (`01_USART_printf`와 동일 기법).
- 폴링 방식이므로 변환 완료까지 CPU가 대기한다.

## 배선

PA0(Arduino A0)에 0~3.3V 신호(가변저항 중간 단자 등)를 연결한다. 연결하지 않으면 플로팅 값이 나온다.

## 동작

200ms마다 `ADC PA0 = <raw 12비트 값> (<mV> mV)` 출력.

## 참고 (다음 단계)

- 내부 온도 센서(채널 16)를 읽어 섭씨로 환산하는 예제는 `02_ADC_Temperature`.
- 실제 VDDA 보정은 `06_ADC_Vrefint_VDDA_c`.

## 관련 예제

`02_ADC_Temperature`(내부 채널), `03_ADC_Multi`(여러 채널), `04_ADC_DMA_TimTrigger_c`(타이머 트리거 + DMA), `05_ADC_AnalogWatchdog_c`(아날로그 워치독).

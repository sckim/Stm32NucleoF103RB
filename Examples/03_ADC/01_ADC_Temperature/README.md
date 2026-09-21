# 01_ADC_Temperature

> 계층: **HAL** (STM32CubeMX 생성 프로젝트)

ADC1의 **내부 온도 센서 채널**을 연속 변환해 원시 값을 UART로 출력한다.

## 핵심 학습 내용

- **ADC 기본 사용 순서**: `HAL_ADCEx_Calibration_Start()`(교정) → `HAL_ADC_Start()` → `HAL_ADC_PollForConversion()` → `HAL_ADC_GetValue()`
- **설정 요점**: 채널 `ADC_CHANNEL_TEMPSENSOR`(내부 채널), 연속 변환 모드 `ContinuousConvMode = ENABLE`, 샘플 시간 13.5 사이클. 내부 센서는 소스 임피던스가 높아 충분한 샘플 시간이 필요하다.
- **ADC 클럭**: `SystemClock_Config`의 `RCC_PERIPHCLK_ADC` 분주로 ADC 클럭을 14MHz 이하로 맞춘다.
- `printf` 리다이렉션(`__io_putchar`)으로 결과 출력 (`01_USART_printf`와 동일 기법).
- 폴링 방식이므로 변환 완료까지 CPU가 대기한다.

## 동작

1초마다 `ADC Temperature = <raw 12비트 값>` 출력 (0~4095).

## 참고 (다음 단계)

이 예제는 원시 값만 출력한다. 온도[°C]로 환산하려면 `T = (V25 - Vsense)/Avg_Slope + 25` 공식(데이터시트: V25 ≈ 1.43V, 기울기 ≈ 4.3mV/°C)이 필요하며, 온도 센서의 개체 편차가 크고 VDDA 변동의 영향도 받는다 → `04_ADC_Vrefint_VDDA_HAL_c`.

## 관련 예제

`02_ADC_DMA_TimTrigger_HAL_c`(타이머 트리거 + DMA), `03_ADC_AnalogWatchdog_HAL_c`(아날로그 워치독).

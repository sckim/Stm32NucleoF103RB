# 02_ADC_Temperature

> 계층: **HAL** — 검토 전 (컴파일만 확인)

ADC1의 **내부 온도 센서 채널(CH16)** 을 읽어 섭씨로 환산하고 UART로 출력한다. `01_ADC_Single`이 외부 핀 PA0(CH0)을 읽는 예제라면, 이 예제는 **핀이 없는 내부 채널**을 읽는다.

## 핵심 학습 내용

- **내부 채널**: ADC1의 채널 16은 온도 센서(`ADC_CHANNEL_TEMPSENSOR`), 채널 17은 내부 기준 전압(`ADC_CHANNEL_VREFINT`)이다. 외부 핀을 쓰지 않으므로 GPIO 설정과 배선이 필요 없다.
- **`TSVREFE` 비트**: `ADC1_CR2.TSVREFE = 1`이어야 센서가 켜진다. HAL이 `HAL_ADC_ConfigChannel()`에서 CH16/CH17을 설정할 때 자동으로 켠다.
- **긴 샘플링 시간**: 온도 센서는 소스 임피던스가 높아 **최소 17.1µs** 샘플링이 필요하다. `ADC_SAMPLETIME_239CYCLES_5`로 설정했고 ADC 클럭 8MHz(PCLK2/8)에서 (239.5 + 12.5) / 8MHz = 31.5µs로 조건을 만족한다. `01_ADC_Single`의 13.5 사이클로는 부족하다.
- **환산 공식** (STM32F103xB 데이터시트)
  - `Vsense = raw × VDDA / 4095` (VDDA = 3.3V 가정)
  - `T[°C] = (V25 − Vsense) / Avg_Slope + 25`
  - V25 ≈ 1.43V(25 °C에서의 출력), Avg_Slope ≈ 4.3mV/°C. **온도가 오르면 전압은 내려간다.**
- **float 없이 계산**: `printf("%f")`는 newlib-nano에서 기본으로 꺼져 있으므로 0.1 °C 단위 정수(`t10`)로 계산해 정수부/소수부로 나눠 출력한다.
- 교정(`HAL_ADCEx_Calibration_Start`) → `HAL_ADC_Start` → `HAL_ADC_PollForConversion` → `HAL_ADC_GetValue` 순서는 `01_ADC_Single`과 같다 (폴링, 연속 변환).

## 동작

1초마다 `ADC TempSensor raw=<0~4095> (<mV> mV) -> <온도> C` 출력. 25 °C 근처에서는 raw ≈ 1750~1800(약 1.43V)이 나온다. 손가락으로 칩을 잠시 만지면 몇 °C 올라가는 것을 볼 수 있다.

## 한계

- 칩마다 V25 편차가 커서 **절대 온도는 몇 °C 이상 틀릴 수 있다.** 변화량 관찰용으로 적합하다.
- VDDA를 3.3V로 가정했다. 실제 VDDA가 다르면 오차가 생기며, 이는 `06_ADC_Vrefint_VDDA_c`에서 Vrefint로 보정한다.
- 센서는 칩 내부 온도(자체 발열 포함)이지 주변 공기 온도가 아니다.

## 관련 예제

`01_ADC_Single`(외부 핀 한 채널), `03_ADC_Multi`(여러 채널, Vrefint 포함), `06_ADC_Vrefint_VDDA_c`(VDDA 보정).

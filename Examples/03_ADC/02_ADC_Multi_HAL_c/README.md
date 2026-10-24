# 02_ADC_Multi_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

> CubeMX(`ADC_Multi.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

DMA 없이 여러 아날로그 채널(A0, A1, A2, 내부 Vrefint)을 폴링으로 차례대로 읽는다. 스캔 모드에 불연속 모드를 더해 "트리거 1번 = 1채널"로 만드는 것이 핵심이다.

## 핵심 학습 내용

- **왜 그냥 스캔 모드로는 안 되는가**: 레귤러 그룹 결과 레지스터(`DR`)는 하나뿐이다. 4채널을 연달아 변환하면 `DR`이 덮어써지고 `EOC`는 시퀀스 끝에서만 서므로 CPU가 중간 값을 놓친다. 그래서 보통 DMA를 쓴다(`03_ADC_DMA_TimTrigger_HAL_c`).
- **불연속 모드**: `CR1.DISCEN=1`, `DISCNUM=000` → 트리거마다 시퀀스의 다음 1채널만 변환. `Start → 변환 완료 대기 → DR 읽기`를 채널 수만큼 반복하면 랭크 순서대로 값을 얻고, 마지막 랭크 다음에는 다시 랭크 1부터 시작한다(RM0008 11.3.10).
- **시퀀스 설정**: `CR1.SCAN=1`, `SQR1.L=3`(4개), `SQR3={CH0, CH1, CH4, CH17}`. 채널마다 샘플 시간을 다르게 줄 수 있다 — 외부 채널 55.5 사이클(6.4µs), Vrefint 239.5 사이클(23.6µs, 최소 17.1µs 요구).
- **HAL 동작 주의**: 스캔 모드(`SQR1.L≥1`)에서는 `HAL_ADC_PollForConversion()`이 `EOC`를 기다리지 않고 최대 변환 시간만큼 소프트웨어로 대기한 뒤 반환한다(`stm32f1xx_hal_adc.c` 주석). `HAL_ADC_Stop()`은 루프 중간에 호출하지 않는다.
- Vrefint로 실제 VDDA를 역산해 외부 채널을 mV로 환산(원리는 `05_ADC_Vrefint_VDDA_HAL_c`).

## 배선

PA0(A0), PA1(A1), PA4(A2)에 가변저항 또는 0~3.3V 신호 (미연결 시 플로팅 값).

## 동작

4채널 스캔을 8회 평균 → 1초마다 UART(115200)로 출력.

```
A0=2048(1650mV) A1=   0(   0mV) A2=4095(3300mV) | Vref=1489 -> VDDA=3300mV
```

## 인터럽트

사용하지 않는다 (폴링 변환). ISR은 `SysTick_Handler`(HAL 틱)뿐이다.

## 관련 예제

`01_ADC_Temperature`(단일 채널 폴링), `03_ADC_DMA_TimTrigger_HAL_c`(스캔 + DMA), `05_ADC_Vrefint_VDDA_HAL_c`(VDDA 보정), `12_Advanced_Topics/01_ADC_Injected_HAL_c`(주입 그룹 JDR1~4).

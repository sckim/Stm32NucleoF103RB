# 04_ADC_DMA_TimTrigger

> 계층: **HAL** + TIM + DMA

> CubeMX(`ADC_DMA_TimTrigger.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

교안: [ADC 타이머 트리거 DMA 교안.html](ADC%20타이머%20트리거%20DMA%20교안.html) (짝 교안: [03_ADC_Multi/ADC 다채널 교안.html](../03_ADC_Multi/ADC%20다채널%20교안.html))

타이머가 정확한 주기(1kHz)로 ADC 변환을 시작시키고, DMA가 결과를 원형 버퍼에 저장한다. CPU는 버퍼가 반/전부 찼을 때만 개입한다.

## 03_ADC_Multi에서 이어지는 점

`03_ADC_Multi`는 여러 채널을 읽을 수는 있지만 두 가지 한계가 있다. 이 예제는 그 둘을 각각 하드웨어로 해결한다.

| 03의 한계 | 원인 | 04의 해결 |
|---|---|---|
| 샘플 간격이 정확하지 않다 | 변환 시작이 소프트웨어(`SWSTART`)라서, 간격이 HAL 대기 루프·UART 송신·`HAL_Delay`에 좌우된다 | **TIM3 TRGO**가 1ms마다 변환을 시작한다. 간격이 타이머 클럭 정밀도로 고정된다 |
| CPU가 매 변환마다 붙잡혀 있다 | DR이 하나뿐이라 덮어쓰기 전에 CPU가 폴링으로 꺼내야 한다 | **DMA**가 매 변환마다 DR을 메모리로 복사한다. CPU는 반/전체 버퍼 콜백에서만 개입한다 |

- **그대로 쓰는 것**: 랭크(`SQR3`), 변환 개수(`SQR1.L`), 채널별 샘플 시간(`SMPR2`, 55.5 사이클), 스캔 모드(`CR1.SCAN=1`), 교정(`CR2.CAL`), 평균 계산.
- **바뀌는 것**: 시작 주체가 소프트웨어에서 타이머로(`CR2.EXTSEL`: `111` SWSTART → `100` TIM3_TRGO), DR을 꺼내 가는 주체가 CPU에서 DMA로(`CR2.DMA=1`) 바뀐다.
- **DMA를 쓰는 이유**: 03은 불연속 모드(`CR1.DISCEN=1`)로 "트리거 1번 = 1채널"을 만들어 CPU가 사이사이 DR을 읽게 했다. 그러나 F1은 시퀀스 중간에 `EOC`가 서지 않으므로, 타이머가 트리거를 주는 구조에서는 CPU에게 "지금 읽어라"를 알릴 인터럽트가 없다. DMA는 EOC와 상관없이 변환이 하나 끝날 때마다(DR 갱신마다) 요청을 받아 결과를 옮기므로, 매 변환을 놓치지 않고 받아 갈 주체로 DMA를 쓴다.
- **불연속 모드를 끄는 이유**: DMA는 불연속 모드를 켜도 정상 동작한다. 이 예제가 불연속 모드를 끄는(`DISCEN=0`) 것은 **매 주기 두 채널을 거의 같은 시점에 읽는 "스냅샷"**을 원하기 때문이다. 트리거 한 번에 시퀀스 전체(2채널)가 약 12.8µs 안에 변환되고, DMA가 결과를 `adc_buf[]`에 차례로 쌓는다. 불연속 모드(n=1)를 켜면 트리거마다 1채널씩 돌아가며 변환하므로 채널당 샘플링 주파수가 1kHz/2 = 500Hz로 떨어지고, 두 채널의 측정 시점이 1ms씩 벌어진다.
- **인터럽트 구조**: 03에는 ADC 관련 ISR과 콜백이 없었다. 04에서는 진짜 ISR `DMA1_Channel1_IRQHandler`가 HAL 콜백(`HAL_ADC_ConvHalfCpltCallback`, `HAL_ADC_ConvCpltCallback`)을 부른다. 아래 "ISR vs 콜백" 참고.
- **단순화한 점**: 채널을 2개(A0, A1)로 줄이고 Vrefint 보정은 뺐다. 03처럼 A2(CH4)와 Vrefint(CH17)를 랭크 3·4에 추가하려면 `SQR1.L=3`으로 바꾸고 `N_CH`를 4로 늘린다. Vrefint 랭크는 샘플 시간을 239.5 사이클로 둔다.

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

`01_ADC_Single`(폴링 ADC), 타이머 기초 `06_Timers_RTC/01_TIM_TimeBase`, DMA 기초 `07_DMA/01_DMA_MemToMem`.

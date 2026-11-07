# 09_ADC_TriggerJitter_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「ADC」(주변장치 3) 6.4절(변환 시작원과 트리거)과 6.7절(신호처리 관점)의 실습이다.
`04_ADC_DMA_TimTrigger`가 "타이머 트리거는 샘플 간격이 정확하다"고 설명한 것을 **숫자로** 확인한다.

## 핵심 학습 내용

- **세 가지 시작 방식**(1 kHz 목표, 1000샘플)
  - A) 소프트웨어 + 고정 지연: `SWSTART` → 가변 작업(0~199 µs) → 1000 µs 지연. 평균 주기가 늘고 지터가 크다.
  - B) 소프트웨어 + 마감 시각: DWT로 다음 마감(+1000 µs)까지 기다린 뒤 `SWSTART`. 평균은 맞지만 끼어드는 인터럽트만큼 흔들린다.
  - C) 하드웨어: `TIM3->CR2.MMS = 010`(갱신 → TRGO), `ADC1->CR2.EXTSEL = 100`(TIM3_TRGO). CPU가 무엇을 하든 시작 시각이 고정된다.
- **측정 방법**: EOC 인터럽트(우선순위 0)에 들어오자마자 `DWT->CYCCNT`를 기록한다. 7.3 kHz로 5 µs씩 도는 방해 인터럽트(TIM2, 우선순위 1)가 실제 응용의 다른 ISR 역할을 한다.
- **ADON 재기록 함정**: ADON = 1인 CR2에 다른 비트를 바꾸지 않고 ADON = 1을 다시 쓰면 그 쓰기가 변환을 시작한다(RM0008 ADC_CR2 ADON 주석). 트리거 선택 함수는 값이 같으면 쓰지 않는다.
- **트리거 순서**: 슬레이브(ADC) 클럭을 마스터(TIM3)보다 먼저 켠다(ES096 2.6.2 TRGO 주의).

## 지터와 SNR

샘플링 시각 지터 σ_t는 주파수 f의 사인파에 대해 `SNR ≤ −20·log10(2π·f·σ_t)`의 상한을 만든다.

| σ_t | f = 50 Hz | f = 500 Hz |
|---|---|---|
| 50 µs | 36 dB | 16 dB |
| 1 µs | 70 dB | 50 dB |
| 50 ns | 96 dB | 76 dB |

12비트 이상적 양자화 SNR은 6.02×12 + 1.76 ≈ 74 dB이다. 1 µs 지터는 500 Hz 성분에서 이미 양자화보다 큰 잡음을 만든다.

## 동작

배선 없음. USART2(115200 8N1)로 약 5초마다 세 줄을 출력한다(값은 예시이다).

```
A) SW + delay    mean  1100.xxx min ... max ... p-p ... sigma ... us
B) SW deadline   mean  1000.000 min ... max ... p-p ...   sigma ... us
C) TIM3 TRGO     mean  1000.000 min ... max ... p-p ...   sigma ... us
```

**A·B·C의 p-p와 σ가 몇 자리씩 차이 나는지 보는 것이 이 실험의 질문이다.** C에 남는 몇 사이클은 ADC 클럭(10.67 MHz)과 코어 클럭의 동기화, 인터럽트 진입 지연의 흔들림이다.

## ISR vs 콜백

- `ADC1_2_IRQHandler`(진짜 ISR, 이 파일): DR을 읽어 EOC를 지우고 시각만 기록한다.
- `TIM2_IRQHandler`(진짜 ISR, 이 파일): UIF를 지우고 5 µs 바쁜 대기.
- HAL 콜백은 없다.

## 관련 예제

`04_ADC_DMA_TimTrigger`(HAL로 TRGO + DMA), `06_Timers_RTC/01_TIM_TimeBase`(TIM3 1 kHz), `08_Clock_System/10_DWT_Profiling_Reg_c`(DWT), `08_Clock_System/15_NVIC_Pending_TailChain_Reg_c`(우선순위와 선점).

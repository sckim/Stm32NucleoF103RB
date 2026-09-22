# 02_DMA_Circular_GPIO_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

**원형(circular) DMA + 타이머 요청**으로 GPIO 포트(`GPIOC->ODR`)에 패턴을 CPU 없이 자동 출력하고, 반 버퍼(HT)/전체(TC) 인터럽트로 **핑퐁(double buffering)** 갱신을 한다.

## 핵심 학습 내용

- **구성**: TIM2 업데이트(10kHz) → DMA 요청(`DIER.UDE`) → DMA1 채널2 → `GPIOC->ODR`(반워드). 64샘플 버퍼가 끝없이 반복(`CCR.CIRC`).
- **핑퐁**: DMA 가 한쪽 절반을 내보내는 동안 CPU 가 다른 절반을 채운다. `HT` = 앞 절반 완료, `TC` = 뒤 절반 완료.
- **콜백 등록**: `HAL_DMA_RegisterCallback`으로 HT/TC 콜백을 등록. 1초마다 패턴(나이트라이더/이진 램프/삼각파)이 바뀐다.
- **주의**: `ODR` 전체(16비트)를 쓰므로 나머지 핀도 구동된다. 필요한 비트만 쓰려면 `BSRR`을 대상으로 삼는다.

## 배선 / 동작

PC0~PC7 에 로직 분석기, 또는 LED 8개(저항 포함), 또는 R-2R 병렬 DAC 를 연결. UART(115200)에 HT/TC 횟수를 출력(각각 약 156/s 기대).

## ISR vs 콜백

`DMA1_Channel2_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → 등록된 `half_cb`/`full_cb`(이 파일).

## 관련 예제

`01_DMA_MemToMem_HAL_c`, `12_Advanced_Topics/06_DMA_Priority_HAL_c`, `03_ADC/02_ADC_DMA_TimTrigger_HAL_c`(원형 DMA 수신 쪽).

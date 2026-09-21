# 60_DMA_MemToMem_HAL_c

> 계층: **HAL** + DMA — Claude 작성, 검토 전 (컴파일만 확인)

256워드(1KB) 배열을 CPU 반복문과 DMA로 각각 복사하고 DWT 사이클 카운터로 시간을 비교한다.

## 핵심 학습 내용

- **DMA 설정** (RM0008 13장): `DMA1_Channel1->CCR`의 `MEM2MEM=1`, `DIR=1`(주변장치 주소 = 소스), `PINC=1`, `MINC=1`, `PSIZE=MSIZE=32비트`. `CPAR` = 소스, `CMAR` = 목적지, `CNDTR` = 전송 개수.
- DMA는 CPU를 쓰지 않고 버스 마스터로 복사하며, 완료되면 `ISR.TCIF1` → 인터럽트.
- **콜백 등록 방식**: `HAL_DMA_RegisterCallback()`으로 `dma_done_cb()`를 등록한다 (다른 예제의 고정 이름 콜백과 다른 방식).
- **성능 측정**: DWT 사이클 카운터로 CPU 복사와 DMA 복사 시간을 비교. 데이터가 SRAM 안에서 왕복하므로 버스 경합 때문에 DMA가 CPU 루프보다 극적으로 빠르지는 않을 수 있다. **이 예제의 핵심은 "전송 중 CPU가 자유로움"을 확인하는 것이다.**

## 동작

부팅 시 1회, 이후 B1(PC13)을 누를 때마다 다시 측정해 UART(115200)로 출력.

## ISR vs 콜백

`DMA1_Channel1_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → `dma_done_cb()`(이 파일, 등록한 콜백).

## 관련 예제

`08_Clock_System/77_DWT_Profiling_Reg_c`(DWT 원리), 주변장치 DMA `02_USART/12`, `03_ADC/21`, `05_SPI/41`.

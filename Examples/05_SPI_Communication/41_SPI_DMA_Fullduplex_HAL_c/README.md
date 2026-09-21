# 41_SPI_DMA_Fullduplex_HAL_c

> 계층: **HAL** + DMA — Claude 작성, 검토 전 (컴파일만 확인)

SPI1 전이중 전송을 DMA에 맡기고, 그 동안 CPU가 다른 일을 한 횟수를 세어 폴링 방식과 비교한다.

## 핵심 학습 내용

- **SPI + DMA 매핑** (RM0008 표 78): `SPI1_RX` → **DMA1 채널2**, `SPI1_TX` → **DMA1 채널3**. `SPI1->CR2.RXDMAEN / TXDMAEN`으로 DMA 요청 허용.
- **채널 설정**: TX는 메모리→주변장치(`SPI1->DR`), RX는 주변장치→메모리, 메모리 주소만 증가, 바이트 단위.
- **폴링 vs DMA 비교**: 512바이트를 8MHz로 전송하는 동안 `free_loops`(CPU가 다른 일을 한 횟수)와 소요 시간을 출력한다. 전송 자체 시간은 비슷해도 DMA에서는 CPU가 자유롭다.
- OLED/TFT/SD 카드처럼 큰 데이터를 보내는 장치에서 DMA가 필수적인 이유.
- 완료 통지는 콜백으로 받고 main은 플래그로 확인.

## 배선

`PA7 (MOSI) ── 점퍼 ── PA6 (MISO)` (`40`과 동일).

## ISR vs 콜백

`DMA1_Channel2_IRQHandler` / `DMA1_Channel3_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → `HAL_SPI_TxRxCpltCallback()`(이 파일): 송수신이 모두 끝났을 때 1회 호출.

## 관련 예제

`40`(폴링), `02_USART/12_USART_DMA_IdleLine_HAL_c`, `07_DMA/60_DMA_MemToMem_HAL_c`.

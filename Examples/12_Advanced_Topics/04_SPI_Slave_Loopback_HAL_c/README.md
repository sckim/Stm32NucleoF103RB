# 04_SPI_Slave_Loopback_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

**SPI 슬레이브 모드**를 보드 한 장에서 시험한다. SPI1(마스터)과 SPI2(슬레이브)를 점퍼선 4개로 연결한다. 슬레이브의 응답이 "직전 프레임에 대한 결과"로 한 프레임 늦게 오는 SPI 특유의 파이프라인 동작을 확인한다.

## 핵심 학습 내용

- **마스터/슬레이브 차이**: 마스터가 SCK 를 만들고 NSS 로 슬레이브를 선택. 전이중이라 슬레이브의 **송신 데이터를 클럭이 오기 전에 미리 장전(arm)** 해 두어야 한다.
- **하드웨어 NSS**: 슬레이브는 `SPI_NSS_HARD_INPUT`으로 NSS 가 Low 일 때만 동작 (`CR1.SSM=0`).
- **재장전 패턴**: 프레임 완료 콜백에서 다음 응답을 만들고 `HAL_SPI_TransmitReceive_IT`를 다시 호출한다.
- **검증**: 마스터가 받은 값이 `(직전에 보낸 값 ^ 0xFF)`인지 검사해 PASS/FAIL 출력 (RM0008 25.3.2 슬레이브 모드 구성).

## 배선 / 동작

PA5→PB13(SCK), PA7→PB15(MOSI), PB14→PA6(MISO), PA4(마스터 CS)→PB12(슬레이브 NSS). PA5 는 LD2 와 같은 핀이라 전송 중 LED 가 흐릿하게 켜진다(정상). UART(115200)로 결과 출력.

## ISR vs 콜백

`SPI2_IRQHandler`(진짜 ISR) → `HAL_SPI_IRQHandler()` → `HAL_SPI_TxRxCpltCallback()`, `HAL_SPI_ErrorCallback()`(이 파일).

## 관련 예제

`05_SPI_Communication/01_SPI_Loopback_HAL_c`, `05_SPI_Communication/02_SPI_DMA_Fullduplex_HAL_c`, `04_I2C_Communication/03_I2C_Slave_Loopback_HAL_c`.

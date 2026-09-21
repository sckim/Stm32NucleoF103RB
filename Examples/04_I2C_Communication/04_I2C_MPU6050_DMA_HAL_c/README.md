# 04_I2C_MPU6050_DMA_HAL_c

> 계층: **HAL + DMA** — Claude 작성, 검토 전 (컴파일만 확인)

`HAL_I2C_Mem_Read_DMA()`로 **논블로킹 I2C**: 읽기를 시작만 하고 CPU 는 다른 일을 하다가 완료 콜백에서 결과를 받는다. `03_I2C_MPU6050_HAL_c`의 블로킹 방식과 대비된다.

## 핵심 학습 내용

- **DMA 채널 매핑**(RM0008 13.3.7): `I2C1_TX` → DMA1 채널6, `I2C1_RX` → DMA1 채널7. `I2C1->CR2.DMAEN`, `LAST`.
- **여러 ISR 이 협업**: 주소/레지스터 단계는 `I2C1_EV/ER`, 데이터 이동은 DMA 인터럽트가 담당한다.
- **오류 복구**: NACK/타임아웃이 나면 `HAL_I2C_ErrorCallback`에서 플래그를 세우고 main 이 I2C 를 재초기화한다 (F1 I2C 는 오류 뒤 재초기화가 안전).
- 출력의 `main loops during DMA`가 전송 중 CPU 가 자유로웠음을 보여 준다.

## 배선 / 동작

MPU6050(GY-521): VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND. 100ms 마다 14바이트를 DMA 로 읽어 가속도/자이로를 UART(115200)로 출력.

## ISR vs 콜백

`DMA1_Channel6/7_IRQHandler`, `I2C1_EV/ER_IRQHandler` → `HAL_DMA_IRQHandler`/`HAL_I2C_EV/ER_IRQHandler` → `HAL_I2C_MemRxCpltCallback()`, `HAL_I2C_ErrorCallback()`(이 파일).

## 관련 예제

`03_I2C_MPU6050_HAL_c`(블로킹), `05_SPI_Communication/02_SPI_DMA_Fullduplex_HAL_c`.

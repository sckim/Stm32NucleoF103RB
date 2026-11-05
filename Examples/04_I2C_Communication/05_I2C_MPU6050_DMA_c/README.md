# 05_I2C_MPU6050_DMA_c

> 계층: **HAL + DMA** — 검토 전 (컴파일만 확인)

교안: [05_I2C_MPU6050_DMA 교안.html](05_I2C_MPU6050_DMA%20교안.html)

> CubeMX(`I2C_MPU6050_DMA.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

`HAL_I2C_Mem_Read_DMA()`로 **논블로킹 I2C**: 읽기를 시작만 하고 CPU 는 다른 일을 하다가 완료 콜백에서 결과를 받는다. `04_I2C_MPU6050_c`의 블로킹 방식과 대비된다.

## 핵심 학습 내용

- **DMA 채널 매핑**(RM0008 13.3.7): `I2C1_TX` → DMA1 채널6, `I2C1_RX` → DMA1 채널7. `I2C1->CR2.DMAEN`, `LAST`.
- **절반만 논블로킹**: HAL F1 V1.8.x 의 `HAL_I2C_Mem_Read_DMA()`는 주소/레지스터 단계(START-0xD0-0x3B-RESTART-0xD1, 약 0.3ms)를 함수 안에서 폴링으로 처리하고, 데이터 14바이트(약 1.3ms)만 DMA1 채널7 이 옮긴다. 완료는 DMA 인터럽트(TC), 오류는 `I2C1_ER` 인터럽트가 알린다. `I2C1_EV`와 DMA1 채널6(TX)은 이 흐름에서 쓰이지 않는다.
- **오류 복구**: 주소 단계 NACK/타임아웃은 `HAL_I2C_Mem_Read_DMA()`의 반환값(HAL_ERROR/HAL_BUSY)으로, 데이터 단계 BERR/ARLO/OVR·DMA 오류는 `HAL_I2C_ErrorCallback`으로, 응답이 아예 없으면 main 의 소프트웨어 타임아웃(20ms)으로 알아채고 main 이 I2C 를 재초기화한다 (F1 I2C 는 오류 뒤 재초기화가 안전).
- 출력의 `main loops during DMA`가 전송 중 CPU 가 자유로웠음을 보여 준다.

## 배선 / 동작

MPU6050(GY-521): VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND. 100ms 마다 14바이트를 DMA 로 읽어 가속도/자이로를 UART(115200)로 출력.

## ISR vs 콜백

ISR: `DMA1_Channel7_IRQHandler`(수신 완료/오류), `I2C1_ER_IRQHandler`(버스 오류) → `HAL_DMA_IRQHandler`/`HAL_I2C_ER_IRQHandler` → HAL 콜백 `HAL_I2C_MemRxCpltCallback()`, `HAL_I2C_ErrorCallback()`(main.c, ISR 문맥). `DMA1_Channel6_IRQHandler`, `I2C1_EV_IRQHandler`는 CubeMX가 함께 켜 두었지만 이 예제에서는 호출되지 않는다.

## 관련 예제

`04_I2C_MPU6050_c`(블로킹), `05_SPI_Communication/02_SPI_DMA_Fullduplex_c`.

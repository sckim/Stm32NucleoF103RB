# 04_SPI_OLED_SSD1306_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

SPI 로 **SSD1306 OLED**를 구동한다 (7핀 SPI 모듈). I2C 예제와 같은 화면을 그리며, 통신 방식 차이가 핵심이다: I2C 는 제어 바이트로, SPI 는 별도 **D/C 핀**으로 명령/데이터를 구분한다.

## 핵심 학습 내용

- **SPI 8MHz** 로 1024바이트 ≈ 1ms → I2C(약 23ms)보다 훨씬 빠른 프레임 속도.
- **제어 핀**: CS(PA4, 소프트웨어), DC(PB0: Low=명령/High=데이터), RES(PB1: 시작 시 Low 펄스 리셋).
- **송신 전용**: `SPI_DIRECTION_1LINE`으로 MISO 핀을 쓰지 않는다 (`CR1.BIDIMODE`).
- SPI 모드 0, MSB 먼저, `BR=/8`(64MHz/8). SSD1306 최대 클럭 약 10MHz 이내.

## 배선 / 동작

OLED(SPI 7핀): GND, VCC=3.3V, SCL=PA5, SDA=PA7, RES=PB1, DC=PB0, CS=PA4. PA5 는 LD2 와 같은 핀이라 LED 가 흐릿하게 켜진다. FPS 를 UART 로도 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다(블로킹 SPI).

## 관련 예제

`04_I2C_Communication/06_I2C_OLED_SSD1306_HAL_c`(동일 화면, I2C), `02_SPI_DMA_Fullduplex_HAL_c`(DMA 전송).

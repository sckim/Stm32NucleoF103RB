# 01_I2C_Scan

> 계층: **HAL**

예제설명: [01_I2C_Scan 예제설명.html](01_I2C_Scan%20예제설명.html)

> CubeMX(`I2C_Scan.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

I2C1 버스에 연결된 슬레이브 주소를 찾는 스캐너.

## 핵심 학습 내용

- **주소 호출 원리**: 7비트 주소 0x08~0x77 각각에 대해 START → `(addr<<1 | 0)` 전송 → 슬레이브가 ACK하면 `SR1.ADDR`, 없으면 `SR1.AF`(NACK) → STOP. `HAL_I2C_IsDeviceReady()`가 이 절차를 수행한다.
- **주소 범위**: 0x00~0x07과 0x78~0x7F는 예약 주소라 스캔에서 제외.
- **HAL 주소 인자** = 7비트 주소를 왼쪽으로 1비트 시프트한 값.
- 100kHz 표준 모드 타이밍: `I2C1->CR2.FREQ`(APB1 클럭 MHz), `CCR`, `TRISE`.
- 풀업이 없거나 SDA/SCL이 Low에 묶이면 `SR2.BUSY`가 풀리지 않아 `HAL_I2C_IsDeviceReady()`가 25ms 뒤 `HAL_BUSY`를 돌려준다. 예제는 이를 NACK(`HAL_ERROR`)와 구별해 `bus error at 0x..: BUSY`를 출력하고 스캔을 중단한다.

## 동작

2초마다 스캔을 반복해 UART(115200)로 발견된 주소를 출력하고, 흔한 장치(PCF8574 0x27, MPU6050 0x68, SSD1306 0x3C 등)는 이름을 함께 표시한다.

## 배선

SCL = PB6('D10'), SDA = PB7(Morpho CN7 핀21), 4.7kΩ 풀업.

## 인터럽트

사용하지 않는다 (폴링).

## 관련 예제

`02_PCF8574_Output`·`03_PCF8574_LCD`(스캔으로 찾은 PCF8574를 다룸, 단 PB8/PB9 리맵), `04_I2C_MPU6050_c`(찾은 0x68로 레지스터 읽기).

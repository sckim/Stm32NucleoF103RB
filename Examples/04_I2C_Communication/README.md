# 04_I2C_Communication — I2C

I2C1(SCL = PB6, SDA = PB7)로 확장 IC의 출력 핀을 다루고, 슬레이브 장치를 찾고(스캔), 문자 LCD를 제어하고, 센서 레지스터를 읽는다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_I2C_Scan](01_I2C_Scan) | HAL | 0x08~0x77 버스 스캐너 |
| 02 | [02_PCF8574_Output](02_PCF8574_Output) | HAL | PCF8574 기본: 1바이트 쓰기 = 핀 8개 출력, 준양방향 핀, 읽어 확인 |
| 03 | [03_PCF8574_LCD](03_PCF8574_LCD) | HAL | 02의 응용: PCF8574로 문자 LCD 구동, 주소 스캔 포함 |
| 04 | [04_I2C_MPU6050_c](04_I2C_MPU6050_c) | HAL | 레지스터 읽기 시퀀스, 정수 단위 변환 |
| 05 | [05_I2C_MPU6050_DMA_c](05_I2C_MPU6050_DMA_c) | HAL, DMA | 논블로킹 I2C(DMA), 오류 복구 |
| 06 | [06_I2C_OLED_SSD1306_c](06_I2C_OLED_SSD1306_c) | HAL | SSD1306 OLED, 프레임버퍼, 애니메이션 |
| 07 | [07_I2C_EEPROM_AT24C_c](07_I2C_EEPROM_AT24C_c) | HAL | AT24Cxx: 16비트 주소, 페이지 경계, ACK 폴링 |
| 08 | [08_I2C_DS3231_RTC_c](08_I2C_DS3231_RTC_c) | HAL | 외장 RTC: BCD 시각, 온도, OSF 플래그 |

## 이 그룹에서 배우는 것

- I2C 프로토콜: 2선(SCL/SDA, **오픈 드레인 + 풀업**), 7비트 주소 + R/W 비트, ACK/NACK
- **HAL 주소 인자 = 7비트 주소를 1비트 왼쪽 시프트한 값** (`0x27` → `0x4E`). 가장 흔한 실수
- 레지스터 기반 장치 접근: `HAL_I2C_Mem_Read/Write` (START - 주소 - 레지스터 - RESTART - 읽기)
- 장치가 응답하지 않을 때의 진단 수단으로서 버스 스캔

**상위 교안**: [04_I2C_Communication 교안.html](04_I2C_Communication%20교안.html)은 I2C 하드웨어를 RM0008의 원본 블록도로 설명한다. 다루는 내용은 오픈드레인 버스와 START/STOP·주소·ACK, SCL 생성(CCR·TRISE)과 상승 시간, 이벤트(EV5~EV8)와 플래그 지우기 순서의 상태기계, 인터럽트 두 줄과 DMA, 정오표(ES096) I2C 항목, 확인 문제이며, 각 예제가 어느 경로를 쓰는지 연결한다. 그림 원본은 `img/` 폴더에 있다.

## 공통 배선

SCL = PB6(Arduino D10), SDA = PB7. 단, `02_PCF8574_Output`과 `03_PCF8574_LCD`는 I2C1을 리맵(`AFIO_MAPR.I2C1_REMAP=1`)해 SCL = PB8(D15), SDA = PB9(D14)를 쓴다. 각 라인에 4.7kΩ 풀업(3.3V)이 필요하며 대부분의 모듈에는 이미 장착되어 있다. 풀업이 없으면 버스가 BUSY 상태에 걸린다.

# 04_I2C_Communication — I2C

I2C1(SCL = PB6, SDA = PB7)로 슬레이브 장치를 찾고(스캔), 문자 LCD를 제어하고, 센서 레지스터를 읽는다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 30 | [30_PCF8574](30_PCF8574/README.md) | HAL | I2C 확장 IC로 문자 LCD 구동, 주소 스캔 포함 |
| 31 | [31_I2C_Scan_HAL_c](31_I2C_Scan_HAL_c/README.md) | HAL | 0x08~0x77 버스 스캐너 |
| 32 | [32_I2C_MPU6050_HAL_c](32_I2C_MPU6050_HAL_c/README.md) | HAL | 레지스터 읽기 시퀀스, 정수 단위 변환 |

## 이 그룹에서 배우는 것

- I2C 프로토콜: 2선(SCL/SDA, **오픈 드레인 + 풀업**), 7비트 주소 + R/W 비트, ACK/NACK
- **HAL 주소 인자 = 7비트 주소를 1비트 왼쪽 시프트한 값** (`0x27` → `0x4E`). 가장 흔한 실수
- 레지스터 기반 장치 접근: `HAL_I2C_Mem_Read/Write` (START - 주소 - 레지스터 - RESTART - 읽기)
- 장치가 응답하지 않을 때의 진단 수단으로서 버스 스캔

## 공통 배선

SCL = PB6(Arduino D10), SDA = PB7. 각 라인에 4.7kΩ 풀업(3.3V)이 필요하며 대부분의 모듈에는 이미 장착되어 있다. 풀업이 없으면 버스가 BUSY 상태에 걸린다.

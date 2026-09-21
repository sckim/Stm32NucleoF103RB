# 32_I2C_MPU6050_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

I2C 레지스터 읽기로 MPU6050(GY-521)의 가속도/자이로/온도를 읽는다.

## 핵심 학습 내용

- **I2C 메모리(레지스터) 읽기 시퀀스** (`HAL_I2C_Mem_Read`)
  `START - [0x68<<1|W] - [레지스터 주소] - RESTART - [0x68<<1|R] - 데이터 N바이트(마지막 NACK) - STOP`
- **센서 부팅 절차**: `WHO_AM_I`(0x75)로 장치 확인(0x68) → `PWR_MGMT_1`(0x6B)=0으로 슬립 해제 (리셋 직후 MPU6050은 슬립 상태).
- **연속 읽기(burst read)**: 0x3B부터 14바이트(ACCEL XYZ, TEMP, GYRO XYZ)를 한 번에 읽어 값들의 시점 일관성을 확보하고 트랜잭션 수를 줄인다. 상위/하위 바이트를 합쳐 부호 있는 16비트로 조립.
- **부동소수점 없는 정수 단위 변환** (기본 범위 ±2g, ±250°/s)
  - 가속도: 16384 LSB/g → `mg = raw × 1000 / 16384`
  - 자이로: 131 LSB/(°/s) → `0.1°/s = raw × 10 / 131`
  - 온도: `T[°C] = raw/340 + 36.53` → `0.01°C = raw × 100 / 340 + 3653`

## 배선

VCC = 3.3V, GND, SCL = PB6, SDA = PB7, AD0 = GND (7비트 주소 0x68).

## 동작

200ms마다 값을 UART(115200)로 출력. 보드를 기울이면 가속도 값이 변하고, 정지 시 중력 방향 축이 약 1000mg이다.

## 인터럽트

사용하지 않는다 (폴링). 비동기로 바꾸려면 `HAL_I2C_Mem_Read_IT/_DMA`와 `HAL_I2C_MemRxCpltCallback`을 사용한다 (ISR: `I2C1_EV_IRQHandler` / `I2C1_ER_IRQHandler` → `HAL_I2C_EV/ER_IRQHandler`).

## 관련 예제

`31_I2C_Scan_HAL_c`(주소 확인 후 사용).

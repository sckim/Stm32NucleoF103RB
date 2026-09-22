# 07_I2C_DS3231_RTC_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

I2C 외장 RTC **DS3231**: BCD 시각 읽기/쓰기, 내장 온도 센서, 전원이 끊겼는지 알려 주는 OSF 플래그. 부팅 시 컴파일 시각으로 설정할 수 있다.

## 핵심 학습 내용

- **BCD 형식**: 십의 자리가 상위 니블 (59초 = `0x59`). `bcd2bin(x) = (x>>4)*10 + (x&0x0F)`.
- **레지스터 맵**: `0x00~0x06`(초, 분, 시, 요일, 일, 월, 년), `0x0F`(OSF 플래그), `0x11/0x12`(온도 MSB 정수, LSB 상위 2비트 = 0.25°C).
- **설정 시점**: OSF=1(전원이 끊겼었음) 이거나 B1 을 누른 채 리셋하면 `__DATE__/__TIME__` 으로 설정. 요일은 Sakamoto 공식으로 계산.
- TCXO 내장이라 내부 RTC(`05_RTC_Calendar`)보다 훨씬 정확하고 코인 셀로 시간을 유지한다.

## 배선 / 동작

DS3231 모듈(ZS-042 등): VCC, GND, SCL=PB6, SDA=PB7, 주소 0x68. 1초마다 날짜/시각/온도를 UART(115200)로 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`06_Timers_RTC/05_RTC_Calendar_HAL_c`(내부 RTC), `06_I2C_EEPROM_AT24C_HAL_c`(같은 보드의 EEPROM).

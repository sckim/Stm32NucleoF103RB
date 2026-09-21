# 07_I2C_EEPROM_AT24C_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

I2C **EEPROM(AT24C32/64 등)** 읽기/쓰기. 16비트 메모리 주소, **페이지 경계**, **ACK 폴링**(쓰기 사이클 대기)이라는 EEPROM 특유의 규칙을 다룬다.

## 핵심 학습 내용

- **16비트 주소**: `HAL_I2C_Mem_Write/Read` 에 `I2C_MEMADD_SIZE_16BIT`. (AT24C02~16 은 8비트)
- **페이지 쓰기**: 한 번에 페이지(AT24C32/64 = 32바이트) 안에서만. 경계를 넘으면 주소가 되감겨 앞 데이터를 덮어쓴다 → 페이지 단위로 쪼개 쓴다.
- **쓰기 사이클(최대 5ms)**: 내부 프로그래밍 중에는 NACK. 고정 delay 대신 `HAL_I2C_IsDeviceReady` 반복(ACK 폴링)으로 끝나는 즉시 진행.
- 테스트: 페이지 경계를 걸치는 40바이트(0x011C~)를 쓰고 읽어 비교, 걸린 시간과 폴링 횟수를 출력.

## 배선 / 동작

AT24Cxx 모듈: VCC=3.3V, GND, SCL=PB6, SDA=PB7, 주소 0x50(A2..A0=000). ZS-042 는 EEPROM 이 0x57 인 경우가 많다(`EE_ADDR` 수정). UART(115200) 출력. 영역 0x0100~0x01FF 만 사용.

## ISR vs 콜백

인터럽트를 사용하지 않는다 (블로킹 API).

## 관련 예제

`02_I2C_Scan_HAL_c`(주소 확인), `08_I2C_DS3231_RTC_HAL_c`(같은 모듈의 RTC), `10_Flash_CRC/01_Flash_Write_HAL_c`(비슷한 쓰기 제약).

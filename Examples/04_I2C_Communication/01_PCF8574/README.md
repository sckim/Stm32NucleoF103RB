# 01_PCF8574

> 계층: **HAL** (STM32CubeMX 생성 프로젝트) + 사용자 LCD 라이브러리

PCF8574 I2C 입출력 확장 IC가 붙은 **문자 LCD(16x2)** 를 I2C로 제어한다. 시작 시 I2C 주소 스캐너도 실행한다.

## 핵심 학습 내용

- **I2C 확장 IC로 병렬 LCD 구동**: PCF8574는 I2C로 받은 1바이트를 8개 출력 핀에 내보내며, LCD 모듈의 4비트 데이터/RS/E/백라이트를 여기에 연결해 핀 수를 2개로 줄인다.
- **핸들 구조체 패턴 (C 언어의 객체 지향 흉내)**: `I2C_LCD_HandleTypeDef lcd1`에 `hi2c`와 `address`를 바인딩하고 `lcd_init`, `lcd_clear`, `lcd_gotoxy(col,row)`, `lcd_puts` 함수에 핸들을 전달한다. HAL이 쓰는 방식과 같다.
- **주소 규칙**: HAL은 8비트 주소를 요구하므로 7비트 `0x27`을 왼쪽 시프트한 `0x4E`를 쓴다 (`lcd1.address = 0x4E`). PCF8574A 계열은 7비트 0x3F(HAL 값 0x7E)이다.
- **주소 스캐너**: `HAL_I2C_IsDeviceReady()`로 주소를 순회해 응답하는 장치를 `Found: 0x4E (7bit: 0x27)`처럼 출력한다. 주소를 모를 때의 첫 진단 수단.
- `printf` 리다이렉션으로 진행 상황을 UART로 출력.

## 배선

PCF8574 LCD 백팩: VCC, GND, SDA = PB7, SCL = PB6.

## 동작

1. 스캔 결과 출력 → 2. LCD 초기화/클리어 → 3. 1행 `STM32 with C Code`, 2행 `No OOP Paradigm` 표시 → 4. LD2 500ms 토글.

## 관련 예제

`02_I2C_Scan_HAL_c`(스캐너만 분리·개선), `03_I2C_MPU6050_HAL_c`(센서 레지스터 읽기).

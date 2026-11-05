# 03_PCF8574_LCD

> 계층: **HAL** (STM32CubeMX 생성 프로젝트) + 사용자 LCD 라이브러리

교안: [03_PCF8574_LCD 교안.html](03_PCF8574_LCD%20교안.html)

PCF8574 I2C 입출력 확장 IC가 붙은 **문자 LCD(16x2)** 를 I2C로 제어한다. 시작 시 I2C 주소 스캐너도 실행한다.

## 02_PCF8574_Output에서 이어지는 점

`02_PCF8574_Output`은 "바이트 하나 쓰기 = 핀 P7~P0 상태"만 확인했다. 이 예제는 같은 바이트의 비트에 LCD 신호를 배정한다.

| PCF8574 핀 | P0 | P1 | P2 | P3 | P4~P7 |
|---|---|---|---|---|---|
| LCD 신호 | RS | RW | E | 백라이트 | D4~D7 |

E 비트를 1→0으로 바꾸는 바이트 두 개를 연달아 보내면 LCD가 D7~D4 니블 하나를 읽는다. 8비트 명령/문자 하나는 니블 2개, 즉 I2C 데이터 4바이트이다. LCD 초기화는 라이브러리 `lcd_init()` 대신 `main.c`의 `lcd_init_4bit()`를 쓴다(라이브러리 초기화는 4비트 전환 때 니블 짝이 어긋나 화면이 켜지지 않는다. 주석 참고).

## 핵심 학습 내용

- **I2C 확장 IC로 병렬 LCD 구동**: PCF8574는 I2C로 받은 1바이트를 8개 출력 핀에 내보내며, LCD 모듈의 4비트 데이터/RS/E/백라이트를 여기에 연결해 핀 수를 2개로 줄인다.
- **핸들 구조체 패턴 (C 언어의 객체 지향 흉내)**: `I2C_LCD_HandleTypeDef lcd1`에 `hi2c`와 `address`를 바인딩하고 `lcd_init`, `lcd_clear`, `lcd_gotoxy(col,row)`, `lcd_puts` 함수에 핸들을 전달한다. HAL이 쓰는 방식과 같다.
- **주소 규칙**: HAL은 8비트 주소를 요구하므로 7비트 `0x27`을 왼쪽 시프트한 `0x4E`를 쓴다 (`lcd1.address = 0x4E`). PCF8574A 계열은 7비트 0x3F(HAL 값 0x7E)이다.
- **주소 스캐너**: `HAL_I2C_IsDeviceReady()`로 주소를 순회해 응답하는 장치를 `Found: 0x4E (7bit: 0x27)`처럼 출력한다. 주소를 모를 때의 첫 진단 수단.
- `printf` 리다이렉션으로 진행 상황을 UART로 출력.

## 배선

PCF8574 LCD 백팩: VCC, GND, SDA = PB9(Arduino D14), SCL = PB8(Arduino D15). I2C1 핀을 리맵(`AFIO_MAPR.I2C1_REMAP = 1`)해서 쓴다. 같은 그룹의 다른 예제(PB6/PB7)와 다르다.

## 동작

1. 스캔 결과 출력 → 2. LCD 초기화/클리어 → 3. 1행 `STM32 with C Code`, 2행 `No OOP Paradigm` 표시 → 4. LD2 500ms 토글.

## 관련 예제

`02_PCF8574_Output`(PCF8574 기본 출력), `01_I2C_Scan`(버스 스캐너, 기본 핀 PB6/PB7), `04_I2C_MPU6050_c`(센서 레지스터 읽기).

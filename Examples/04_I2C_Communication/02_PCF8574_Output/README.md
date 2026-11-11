# 02_PCF8574_Output

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, `PCF8574_Output.ioc`). 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `03_PCF8574_LCD`의 것을 복사해 프로젝트 이름만 바꿨다.

예제설명: [02_PCF8574_Output 예제설명.html](02_PCF8574_Output%20예제설명.html)

I2C 입출력 확장 IC **PCF8574**의 가장 기본 동작을 시험한다. I2C로 1바이트를 쓰면 그 비트가 그대로 핀 P7~P0에 나온다. 쓴 뒤 다시 읽어 실제 핀 레벨과 비교한다. 다음 예제 `03_PCF8574_LCD`는 이 원리로 문자 LCD를 구동한다.

## 핵심 학습 내용

- **바이트 하나 = 핀 8개**: `HAL_I2C_Master_Transmit(&hi2c1, 0x4E, &value, 1, 10)` 한 번으로 P0(비트0)~P7(비트7)이 바뀐다. 데이터 바이트의 ACK 직후 핀이 바뀐다.
- **준양방향(quasi-bidirectional) 핀**: 비트 0은 강한 Low(전류 흡수), 비트 1은 약 100µA의 약한 풀업이다. 그래서 LED는 **VCC → 저항 → LED → Px** 로 연결하고 **0을 쓰면 켜진다(액티브 Low)**.
- **읽기 = 실제 핀 레벨**: `HAL_I2C_Master_Receive`로 읽은 값은 출력 래치가 아니라 핀 전압이다. 1을 써 둔 핀을 외부에서 Low로 누르면 읽은 값이 달라진다. 이것이 PCF8574 핀을 입력으로 쓰는 방법이다.
- **주소**: A2=A1=A0=1이면 7비트 0x27, HAL 인자 `0x27 << 1 = 0x4E`. 모두 GND면 0x20(HAL 0x40). PCF8574A는 0x38~0x3F.
- 시작 시 `HAL_I2C_IsDeviceReady`로 ACK를 확인하고, 실패하면 배선 점검 메시지를 출력한다.

## 배선

| PCF8574 | 연결 |
|---|---|
| SCL | PB8 (Arduino D15), 4.7kΩ 풀업 → 3.3V |
| SDA | PB9 (Arduino D14), 4.7kΩ 풀업 → 3.3V |
| A0, A1, A2 | VCC (주소 0x27) |
| P0~P7 | 각각 LED 캐소드. LED 애노드 → 330Ω~1kΩ → VCC |
| VDD / VSS | 3.3V / GND |

I2C1을 리맵(`AFIO_MAPR.I2C1_REMAP = 1`)해 PB8/PB9를 쓴다. `03_PCF8574_LCD`와 같은 핀이라 회로를 그대로 이어 쓸 수 있다. Proteus에서는 LED 대신 로직 프로브를 P0~P7에 달아도 된다.

## 동작

300ms마다 다음 패턴을 차례로 쓰고, 다시 읽어 UART2(115200)로 출력한다. `*`가 켜진 LED이다.

```
ACK: PCF8574 found
W=0xFF R=0xFF  P7..P0=11111111  LED=--------
W=0x00 R=0x00  P7..P0=00000000  LED=********
W=0xFE R=0xFE  P7..P0=11111110  LED=-------*
W=0xFD R=0xFD  P7..P0=11111101  LED=------*-
...
W=0xAA R=0xAA  P7..P0=10101010  LED=-*-*-*-*
W=0x55 R=0x55  P7..P0=01010101  LED=*-*-*-*-
----
```

LD2(PA5)는 패턴마다 토글한다.

## ISR vs 콜백

I2C는 폴링 방식이라 I2C ISR과 콜백이 없다. 활성 ISR은 `SysTick_Handler`(HAL 틱)와 CubeMX 기본 설정의 `EXTI15_10_IRQHandler`(B1)뿐이며, `HAL_GPIO_EXTI_Callback`을 재정의하지 않으므로 B1은 아무 일도 하지 않는다.

## 관련 예제

`03_PCF8574_LCD`(같은 PCF8574로 문자 LCD 구동), `01_I2C_Scan`(버스 전체 주소 스캔), `01_GPIO/04_BlinkReg`(MCU 핀으로 직접 LED 구동과 비교).

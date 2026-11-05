# 08_USART_printf_Dual

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, `01_USART_printf` 기반)

`printf` 출력을 **USART2(ST-Link 가상 COM)** 와 **USART1(PA9 = Arduino D8)** 로 동시에 보낸다. PC 터미널로 글자를 확인하는 동시에, Analog Discovery 2(AD2)로 D8 핀의 UART 파형을 직접 관찰하는 것이 목적이다.

## 왜 USART1을 추가하는가

NUCLEO-F103RB 기본 설정에서 USART2(PA2/PA3)는 ST-LINK 쪽에만 연결되어 있다.

| 솔더 브리지 | 기본 | 의미 |
|---|---|---|
| SB13, SB14 | ON | PA2/PA3 ↔ ST-LINK VCP (PC 터미널) |
| SB62, SB63 | **OFF** | PA2/PA3 ↔ Arduino D1/D0 **끊김** |

따라서 D1에 프로브를 대도 신호가 보이지 않는다(PA2는 Morpho CN10-35에서만 측정할 수 있다). USART1은 Arduino 헤더에 바로 연결되어 있어 측정하기 쉽다.

| USART | TX | RX | 버스 | 용도 |
|---|---|---|---|---|
| USART2 | PA2 | PA3 | APB1 (32 MHz) | PC 터미널 |
| USART1 | **PA9 = D8** | PA10 = D2 | APB2 (64 MHz) | AD2 측정 |

## 핵심 학습 내용

- 하나의 `__io_putchar`에서 두 UART로 같은 문자를 보낸다. 출력 대상(채널)을 늘리는 가장 단순한 방법이다.
- USART1은 APB2, USART2는 APB1에 속하므로 `USARTx_BRR` 값이 다르다. `HAL_UART_Init`이 각 버스의 클럭(PCLK2, PCLK1)으로 BRR을 계산하므로 두 포트 모두 115200bps가 된다.
- 폴링 송신을 두 번 하므로 문자당 송신 시간이 약 2배(≈174 µs)가 된다. 블로킹 송신의 비용을 체감할 수 있다.
- 인터럽트와 콜백은 없다. ISR은 `SysTick_Handler`(HAL_Delay용)뿐이다.

## 배선 (AD2)

| AD2 | NUCLEO |
|---|---|
| DIO 0 (Logic) 또는 1+ (Scope CH1) | **D8 (PA9)** |
| GND (검정), 1− | GND |

## WaveForms 설정

신호 특성: 1비트 8.68 µs, 1문자 ≈ 87 µs, 유휴 상태는 High(3.3 V)이다. 1초마다 몇 ms 길이의 송신 묶음이 한 번 나타난다.

**Logic (권장)**
1. `+` → **UART** 추가, DIO 0, **115200 bps, 8 bit, Parity None, Polarity Standard**
2. Trigger: DIO 0 **Fall**, Mode **Normal** (Auto로 두면 대부분 High 직선만 보인다)
3. Base는 약 10 ms 이상으로 두면 한 번의 송신 묶음 전체가 보인다
4. 디코더에 `Hello World!` … 가 표시되면 PC 터미널 출력과 비교한다

**Scope**
- Trigger: CH1, Edge **Falling**, Level ≈ 1.6 V, Mode **Normal** (또는 Single)
- 전체 묶음은 1~2 ms/div, 비트 단위는 20 µs/div
- Range 1 V/div, Offset ≈ −1.6 V

## 비트 단위 관찰

`main.c`의 USER CODE 3 영역에서 두 `printf` 줄을 주석 처리하고 `printf("A\n");`만 남기면 파형이 짧아져 보기 쉽다. 이때 D8에는 `41 0D 0A` 세 프레임이 나간다 (훅이 `\n` 앞에 `\r`을 붙인다). `'A'` = 0x41은 LSB부터 `Start(0) 1 0 0 0 0 0 1 0 Stop(1)` 순서로 전송된다.

## 동작

부팅 1초 후부터 1초마다 다음 두 줄을 USART2와 USART1로 동시에 출력한다.

```
Hello World!
Hello, everyone! index = N
```

## 확인 방법

- PC: ST-Link COM 포트를 **115200 8N1**로 연다 (`platformio.ini`의 `monitor_port`는 각자 환경에 맞게 수정).
- AD2: 위 설정으로 D8을 관찰한다.

## 관련 예제

- 기반: `01_USART_printf`
- 확장: AD2의 Protocol → UART 도구에서 DIO TX를 D2(PA10, USART1_RX)에 연결하면 수신 실험도 할 수 있다. 이때 MCU 쪽 수신 코드는 `02_USART_Interrupt`를 참고한다.

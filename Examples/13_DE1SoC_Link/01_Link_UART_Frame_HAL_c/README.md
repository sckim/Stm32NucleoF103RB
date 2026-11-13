# 01_Link_UART_Frame_HAL_c

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, `02_USART/08_USART_printf_Dual` 기반)

1초마다 글자 `A`(0x41) 한 바이트를 **USART1(PA9 = D8)** 로 보낸다. DE1-SoC의 `fpga/lab1_top.v`가 이 프레임을 LEDR9~LEDR0에 비트 그대로 켜고, SignalTap으로 파형을 캡처한다. 같은 바이트를 USART2(PC 터미널)에도 보내 송신 여부를 확인한다.

절차는 [실습 매뉴얼](../13_DE1SoC_Link%20실습매뉴얼.html)의 「실습 1」에 있다.

## 원본 예제에서 바뀐 것

`.ioc`와 CubeMX 생성 코드는 그대로이고 `main.c`의 `USER CODE` 영역만 고쳤다.

| 영역 | 변경 |
|---|---|
| `USER CODE 1` | 쓰지 않는 변수 `index` 삭제 |
| `USER CODE 3` | `printf` 두 줄 대신 `HAL_UART_Transmit`으로 한 바이트 송신 |

한 바이트만 보내는 이유는 FPGA의 LED가 마지막으로 받은 프레임만 남기기 때문이다. `printf("A\n")`은 0x41, 0x0D, 0x0A 세 바이트가 나가서 LED에 0x0A(`0010100001`)만 남는다.

## 배선

| Nucleo | DE1-SoC JP1 |
|---|---|
| D8 (PA9, USART1_TX) | 1번 핀 (GPIO_0[0], PIN_AC18) |
| GND | 12번 핀 (GND) |

## 사용 레지스터

- `USART1_BRR`, `USART1_CR1`(TE/RE/UE): 115200 bps 8N1. USART1은 APB2(64 MHz) 소속이다.
- `USART1_SR.TXE`, `USART1_SR.TC`, `USART1_DR`: `HAL_UART_Transmit`이 폴링으로 쓴다.

## 인터럽트

사용하지 않는다. ISR은 `SysTick_Handler`(HAL_Delay용)뿐이고 HAL 콜백은 없다.

## 기대 결과

- FPGA의 LEDR9~LEDR0 = `0100000101` (시작 0, 0x41을 LSB부터, 정지 1)
- PC 터미널(115200 8N1)에 1초마다 `A`가 줄바꿈 없이 이어서 찍힌다

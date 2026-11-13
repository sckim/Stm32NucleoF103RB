# 02_Link_UART_Echo_HAL_c

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, `02_USART/08_USART_printf_Dual` 기반)

PC 터미널에서 친 글자를 MCU가 **USART1**으로 FPGA에 보내고, FPGA(`fpga/lab2_top.v`)가 돌려보낸 바이트를 PC 터미널에 출력한다. FPGA는 받은 바이트를 HEX1~HEX0에 16진수로 표시하고 1을 더해 돌려보낸다.

절차는 [실습 매뉴얼](../13_DE1SoC_Link%20실습매뉴얼.html)의 「실습 2」에 있다.

## 원본 예제에서 바뀐 것

| 영역 | 변경 |
|---|---|
| `USER CODE 0` | `__io_putchar`가 USART2로만 송신한다. USART1로도 내보내면 FPGA가 그 글자를 돌려보내 출력이 끝없이 되풀이된다 |
| `USER CODE 1` | 쓰지 않는 변수 `index` 삭제 |
| `USER CODE 3` | 두 포트의 수신 플래그를 번갈아 확인하는 폴링 루프 |

## 배선

| Nucleo | DE1-SoC JP1 |
|---|---|
| D8 (PA9, USART1_TX) | 1번 핀 (GPIO_0[0], PIN_AC18) |
| D2 (PA10, USART1_RX) | 2번 핀 (GPIO_0[1], PIN_Y17) |
| GND | 12번 핀 (GND) |

## 사용 레지스터

- `USARTx_SR.RXNE`, `USARTx_DR`: `HAL_UART_Receive(…, Timeout = 0)`이 RXNE가 1일 때만 DR을 읽는다.
- `USARTx_SR.TXE`, `USARTx_SR.TC`, `USARTx_DR`: `HAL_UART_Transmit`이 폴링으로 쓴다.

## 인터럽트

사용하지 않는다. ISR은 `SysTick_Handler`(HAL_Delay용)뿐이고 HAL 콜백은 없다.

## 기대 결과

터미널에 `A`를 입력하면 HEX1~HEX0에 `41`이 보이고 터미널에 다음이 찍힌다.

```
PC   -> FPGA : 0x41
FPGA -> MCU  : 0x42 'B'
```

## 한계

`printf`가 출력하는 동안(약 2 ms) 두 바이트 이상이 들어오면 앞의 것을 잃는다. 빠르게 연속으로 받으려면 `02_USART/02_USART_Interrupt`, `03_USART_RingBuffer` 방식으로 바꾼다.

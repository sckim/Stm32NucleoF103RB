# 01_USART_printf

> 계층: **HAL** (STM32CubeMX 생성 프로젝트)

`printf`의 출력을 USART2(ST-Link 가상 COM 포트)로 보내 PC 터미널에서 확인한다. 이후 모든 예제의 디버깅 출력 방식의 기본이 된다.

## 핵심 학습 내용

- **`printf` 리다이렉션**: newlib의 `printf`는 최종적으로 `__io_putchar(int ch)`를 문자마다 호출한다. 이 함수를 재정의해 `HAL_UART_Transmit`으로 내보내면 `printf`가 UART 출력이 된다. (GCC는 `__io_putchar`, 다른 컴파일러는 `fputc`)
- `\n` 앞에 `\r`을 붙여 전송해 터미널에서 줄 바꿈이 맞게 표시되도록 한다 (CR+LF).
- USART2 설정: 9600bps, 8N1, TX/RX 모드 (`huart2.Init`).
- `sprintf`로 문자열을 만들고 `printf("%s", …)`로 출력하는 방법. 
- `HAL_UART_Transmit`은 **블로킹(폴링)** 전송이다. 긴 문자열을 자주 보내면 그 시간 동안 CPU가 묶인다 → `03_USART_DMA_IdleLine_HAL_c`, `04_USART_RingBuffer_HAL_c`에서 개선.

## 동작

부팅 1초 후부터 1초마다 `Hello, everyone! index = N` 출력 (`index`는 0부터 증가).

## 확인 방법

시리얼 터미널을 ST-Link COM 포트, **9600 8N1**로 연다.

## 관련 예제

수신 쪽은 `02_USART_Interrupt_RX_HAL_c`, 이 출력 기법을 사용하는 `03_ADC/01_ADC_Temperature`.

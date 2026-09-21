# 02_USART — 시리얼 통신

ST-Link 가상 COM 포트(USART2: PA2=TX, PA3=RX)로 PC와 통신한다. 단순 출력에서 시작해 수신 방식을 **폴링 → 인터럽트 → DMA → 링 버퍼 → 명령줄(CLI)** 로 발전시킨다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_USART_printf](01_USART_printf/README.md) | HAL | `printf`를 UART로 리다이렉션 (9600bps) |
| 02 | [02_USART_Interrupt_RX_HAL_c](02_USART_Interrupt_RX_HAL_c/README.md) | HAL, NVIC | 1바이트 수신 인터럽트, 콜백→플래그→main |
| 03 | [03_USART_DMA_IdleLine_HAL_c](03_USART_DMA_IdleLine_HAL_c/README.md) | HAL, DMA | DMA 원형 수신 + IDLE 라인으로 가변 길이 패킷 |
| 04 | [04_USART_RingBuffer_HAL_c](04_USART_RingBuffer_HAL_c/README.md) | HAL | ISR→main lock-free 링 버퍼, 오버플로 실험 |
| 05 | [05_UART_CLI_HAL_c](05_UART_CLI_HAL_c/README.md) | HAL | 줄 편집, 명령 테이블 파서 |
| 06 | [06_USART_HalfDuplex_HAL_c](06_USART_HalfDuplex_HAL_c/README.md) | HAL | 단선 반이중, 방향 전환, 오픈드레인 버스 |

## 이 그룹에서 배우는 것

- 디버깅 수단으로서의 `printf` 리다이렉션 (`__io_putchar`)
- 수신 처리 방식의 선택 기준: CPU 부담(바이트당 인터럽트 vs 패킷당 1회), 데이터 유실 가능성, 구현 복잡도
- **ISR과 콜백의 역할 구분**: 진짜 ISR은 `stm32f1xx_it.c`, 사용자 코드는 HAL 콜백에서 "플래그만 세우고" 실제 처리는 main
- 생산자-소비자 패턴과 동기화 없는 자료구조(lock-free)

## 공통

터미널: 115200 8N1 (`01_USART_printf`만 9600). 별도 배선 없음 (보드 USB로 충분).

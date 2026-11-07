# 02_USART — 시리얼 통신

ST-Link 가상 COM 포트(USART2: PA2=TX, PA3=RX)로 PC와 통신한다. 단순 출력에서 시작해 수신 방식을 **폴링 → 인터럽트 → DMA → 링 버퍼 → 명령줄(CLI)** 로 발전시킨다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_USART_printf](01_USART_printf) | HAL | `printf`를 UART로 리다이렉션 (115200bps) |
| 02 | [02_USART_Interrupt](02_USART_Interrupt) | HAL, NVIC | 1바이트 수신 인터럽트, 콜백→플래그→main |
| 03 | [03_USART_RingBuffer](03_USART_RingBuffer) | HAL | ISR→main lock-free 링 버퍼, 오버플로 실험 |
| 04 | [04_USART_DMA_IdleLine_c](04_USART_DMA_IdleLine_c) | HAL, DMA | DMA 원형 수신 + IDLE 라인으로 가변 길이 패킷 |
| 05 | [05_UART_CLI_c](05_UART_CLI_c) | HAL | 줄 편집, 명령 테이블 파서 |
| 06 | [06_USART_HalfDuplex_c](06_USART_HalfDuplex_c) | HAL | 단선 반이중, 방향 전환, 오픈드레인 버스 |
| 07 | [07_USART_Stack](07_USART_Stack) | HAL, CMSIS | 스택(LIFO) 괄호 검사기, ISR 공유 시 경쟁 상태와 PRIMASK 임계 구역 |
| 08 | [08_USART_printf_Dual](08_USART_printf_Dual) | HAL | `printf`를 USART2(VCP)+USART1(PA9=D8)로 동시 출력, AD2로 UART 파형 관찰 |
| 09 | [09_USART_ErrorFlags_Reg_c](09_USART_ErrorFlags_Reg_c) | 레지스터 | TXE·TC 시각, 보레이트 오차와 FE·NE(허용 오차), 오버런 ORE 실측 |

09는 강의자료 교안 「직렬 통신」 장에 맞춰 추가한 레지스터 수준 보충 예제이다.

## 이 그룹에서 배우는 것

- 디버깅 수단으로서의 `printf` 리다이렉션 (`__io_putchar`)
- 수신 처리 방식의 선택 기준: CPU 부담(바이트당 인터럽트 vs 패킷당 1회), 데이터 유실 가능성, 구현 복잡도
- **ISR과 콜백의 역할 구분**: 진짜 ISR은 `stm32f1xx_it.c`, 사용자 코드는 HAL 콜백에서 "플래그만 세우고" 실제 처리는 main
- 생산자-소비자 패턴과 동기화 없는 자료구조(lock-free)
- FIFO(링 버퍼)와 LIFO(스택)의 차이, 공유 변수를 두 문맥이 함께 수정할 때의 경쟁 상태와 임계 구역

**상위 교안**: [02_USART 교안.html](02_USART%20교안.html)은 USART 하드웨어를 RM0008의 원본 블록도로 설명한다. 다루는 내용은 프레임과 오버샘플링, 보레이트 계산, 플래그·인터럽트·DMA, 설계상 한계, 확인 문제이며, 각 예제가 어느 경로를 쓰는지 연결한다. 그림 원본은 `img/` 폴더에 있다.

교안(HTML): 각 예제 폴더의 `<예제명> 교안.html` (폴더명 끝의 `_c`는 파일명에서 뺀다. 예: `04_USART_DMA_IdleLine_c/04_USART_DMA_IdleLine 교안.html`). 03과 07은 `03_USART_RingBuffer/UART 링 버퍼 교안.html`, `07_USART_Stack/스택 교안.html`이다.

## 공통

터미널: 115200 8N1. 별도 배선 없음 (보드 USB로 충분). 단, `08_USART_printf_Dual`의 파형 관찰에는 AD2를 D8(PA9)과 GND에 연결한다.

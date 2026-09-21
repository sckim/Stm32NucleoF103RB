# 13_USART_RingBuffer_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

수신 인터럽트(생산자)가 링 버퍼에 채우고 main(소비자)이 비우는 **lock-free 생산자-소비자** 구조.

## 핵심 학습 내용

- **문제**: 인터럽트는 언제 올지 모르고 main은 다른 일을 할 수 있다. 둘 사이를 잇는 버퍼가 없으면 데이터를 잃는다.
- **링 버퍼 규칙**
  - `head`는 생산자(ISR)만, `tail`은 소비자(main)만 수정 → 단일 생산자/단일 소비자에서는 인터럽트를 막지 않아도 안전 (lock-free).
  - 크기를 2의 거듭제곱(128)으로 잡아 `& (SIZE-1)` 마스크 한 번으로 인덱스를 되감는다.
  - full = `(head+1) & mask == tail`, empty = `head == tail` (한 칸을 비워 둠).
  - **데이터를 쓴 뒤에** `head`를 갱신해야 소비자가 미완성 데이터를 읽지 않는다.
- **오버플로 실험**: B1(PC13)을 누르면 main이 2초간 `HAL_Delay`로 멈춘다. 그 사이 터미널에 127바이트 이상 붙여넣으면 버퍼가 가득 차 `dropped`가 늘어난다 → 버퍼 크기와 처리 주기 설계의 필요성.
- 통계 `[rx=N, dropped=M, max_used=K]`(엔터 입력 시 출력)로 버퍼 사용량 관찰.

## ISR vs 콜백

`USART2_IRQHandler`(진짜 ISR) → `HAL_UART_IRQHandler()` → `HAL_UART_RxCpltCallback()`(이 파일: `rb_push`). 관련 하드웨어: `USART2->CR1.RXNEIE`, `SR.RXNE`, `DR`.

## 관련 예제

`11`(단일 변수 방식의 한계), `14_UART_CLI_HAL_c`(이 링 버퍼 위에 CLI 구축), `12`(DMA 방식).

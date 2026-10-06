# 02_USART_Interrupt_RX_HAL_c

> 계층: **HAL** + NVIC — Claude 작성, 검토 전 (컴파일만 확인)

USART2 수신 인터럽트로 1바이트씩 받아 에코하고 LED를 제어한다.

## 핵심 학습 내용

- **인터럽트 기반 수신**: `HAL_UART_Receive_IT(&huart2, &rx_byte, 1)`이 `USART2->CR1.RXNEIE`(수신 인터럽트)와 `CR3.EIE`를 켠다. 폴링과 달리 데이터가 올 때까지 CPU가 기다리지 않는다.
- **ISR → 콜백 → main 구조**
  - `USART2_IRQHandler`(`stm32f1xx_it.c`, 진짜 ISR) → `HAL_UART_IRQHandler()`(SR의 RXNE/ORE 해석) → `HAL_UART_RxCpltCallback()`(사용자 콜백)
  - 콜백은 ISR 문맥이므로 **데이터와 플래그(`rx_ready`)만 저장**하고, 실제 처리(LED 제어, 에코 송신)는 main 루프에서 한다.
- **재무장(re-arm)**: 1바이트 수신이 끝나면 인터럽트 수신이 종료되므로 콜백에서 `HAL_UART_Receive_IT`를 다시 호출해야 다음 바이트를 받는다.
- `volatile` 변수로 ISR과 main 간 데이터를 공유하는 이유.

## 동작

터미널(115200 8N1)에서 `1` = LD2 ON, `0` = OFF, `t` = 토글. 입력한 문자는 에코된다.

## 한계와 다음 단계

빠르게 연속 입력하면 콜백이 main보다 먼저 다음 바이트를 덮어쓸 수 있다 (단일 변수). → `04_USART_RingBuffer_HAL_c`에서 버퍼로 해결.

# 12_USART_DMA_IdleLine_HAL_c

> 계층: **HAL** + DMA — Claude 작성, 검토 전 (컴파일만 확인)

DMA가 수신 데이터를 버퍼에 채우고, 라인이 조용해지는 순간(**IDLE**) 한 번의 이벤트로 패킷 길이를 알려 준다.

## 핵심 학습 내용

- **바이트당 인터럽트 → 패킷당 인터럽트**: `11`은 바이트마다 CPU가 호출됐지만, DMA는 CPU 개입 없이 메모리에 쓰고 IDLE 이벤트 때만 CPU를 부른다.
- **가변 길이 수신 문제 해결**: 길이를 미리 알 수 없을 때 "마지막 수신 후 1프레임 시간 동안 무신호"(`USART2->SR.IDLE`)를 패킷 끝으로 본다. `HAL_UARTEx_ReceiveToIdle_DMA()` 사용.
- **하드웨어 대응**: USART2_RX → **DMA1 채널6**(`CPAR = &USART2->DR`, `CMAR` = 버퍼), `CR3.DMAR`=1, `CR1.IDLEIE`=1, DMA `CCR.CIRC`=1(원형).
- **원형 버퍼의 위치 추적**: 콜백의 `Size`는 버퍼 내 현재 위치이므로 `old_pos`와 비교해 새로 들어온 구간(래핑 포함)을 복사한다.
- 반 버퍼(HT) 인터럽트는 `__HAL_DMA_DISABLE_IT`로 꺼서 IDLE/TC만 받는다.

## ISR vs 콜백

`USART2_IRQHandler` / `DMA1_Channel6_IRQHandler`(진짜 ISR) → `HAL_UART_IRQHandler` / `HAL_DMA_IRQHandler` → `HAL_UARTEx_RxEventCallback()`(이 파일). 콜백에서는 패킷 복사와 플래그만 처리하고 출력은 main에서 한다. main이 못 따라가 버려진 패킷은 `pkt_dropped`로 센다.

## 동작

터미널에서 문자열을 한 번에 보내면(한 줄 붙여넣기 등) `[RX n bytes] ...`를 출력한다. 이벤트 횟수(`event_count`)를 통해 패킷당 1회임을 확인할 수 있다.

## 관련 예제

`11`(바이트별 인터럽트), `13`(링 버퍼), DMA 기초 `07_DMA/60_DMA_MemToMem_HAL_c`.

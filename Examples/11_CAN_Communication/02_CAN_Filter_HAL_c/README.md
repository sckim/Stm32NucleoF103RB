# 02_CAN_Filter_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

bxCAN **수신 필터(acceptance filter)**: ID 리스트/마스크 모드, 16/32비트 스케일, 표준/확장 프레임, FIFO0/FIFO1 분배. 루프백 모드라 트랜시버 없이 시험한다.

## 핵심 학습 내용

- **뱅크0(32비트 ID 리스트)**: 표준 0x100, 0x200 정확 일치 → FIFO0. **뱅크1(16비트 마스크)**: 표준 0x3xx 데이터 프레임 → FIFO1. **뱅크2(32비트 마스크)**: 확장 ID[28:16]=0x18FF → FIFO0.
- **레지스터 인코딩**(RM0008 24.7.4): 32비트 `STDID<<21 | EXTID<<3 | IDE<<2 | RTR<<1`, 16비트 `STDID<<5 | RTR<<4 | IDE<<3`. 마스크의 1 은 "반드시 일치", 0 은 "무시".
- **결과 표**: 7개 프레임(표준 5 + 확장 2)을 보내고 각각 FIFO0/FIFO1/필터링됨으로 분류해 UART 에 출력. 기대: 0x400 과 EXT 0x1ABCDEF0 은 수신되지 않음.
- 필터로 CPU 가 처리할 프레임 수를 하드웨어가 줄이는 것이 목적이다.

## 배선 / 동작

배선 없음(루프백). UART(115200)에 2초마다 표 출력. 실제 버스에서는 `CAN_MODE_NORMAL` + 트랜시버 + 종단 저항이 필요하다(`01_CAN_Loopback_HAL_c` 참고).

## ISR vs 콜백

`USB_LP_CAN1_RX0_IRQHandler`/`CAN1_RX1_IRQHandler`(진짜 ISR) → `HAL_CAN_IRQHandler()` → `HAL_CAN_RxFifo0/1MsgPendingCallback()`(이 파일).

## 관련 예제

`01_CAN_Loopback_HAL_c`.

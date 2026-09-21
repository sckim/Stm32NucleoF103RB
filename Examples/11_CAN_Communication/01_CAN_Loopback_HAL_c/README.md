# 01_CAN_Loopback_HAL_c

> 계층: **HAL** + bxCAN — Claude 작성, 검토 전 (컴파일만 확인)

CAN1을 **루프백 모드**로 설정해 트랜시버나 상대 노드 없이 송수신을 시험한다.

## 핵심 학습 내용

- **비트 타이밍** (RM0008 24장, CAN 클럭 = APB1 = 32MHz): Prescaler 4 → `tq = 125ns`. 1비트 = Sync(1) + BS1(13) + BS2(2) = 16tq = 2µs → **500kbps**. 샘플 포인트 = (1+13)/16 = 87.5%, SJW = 1tq.
- **초기화 절차**: `CAN1->MCR.INRQ`(초기화 요청) → 초기화 모드에서 `BTR` 설정, `BTR.LBKM=1`(루프백).
- **필터 뱅크**: `FMR/FA1R/FM1R/FS1R/F0R1,F0R2`로 뱅크 0을 "32비트 마스크 모드, 마스크 0 = 모든 ID 통과"로 설정. 필터를 설정하지 않으면 수신되지 않는다.
- **송신**: 3개의 메일박스(`TSR`, `TIxR`, `TDLxR/TDHxR`). 표준 ID `0x123`, 8바이트(증가하는 카운터) 프레임을 500ms마다 송신.
- **루프백의 의미**: 송신 프레임이 내부에서 수신 경로로 되돌아오므로 FIFO0 수신 인터럽트(`IER.FMPIE0`)가 걸린다. 수신한 프레임을 송신 내용과 비교해 UART(115200)로 출력.

## 실제 버스에 연결하려면

1. `CAN_MODE_LOOPBACK` → `CAN_MODE_NORMAL`로 변경 (코드의 `CAN_TEST_MODE`)
2. 3.3V CAN 트랜시버(SN65HVD230 등)를 PA12(CAN_TX), PA11(CAN_RX)에 연결, 버스 양 끝 120Ω 종단
3. 상대 노드도 500kbps로 설정

**주의**: PA11/PA12는 USB와 같은 핀이고, bxCAN과 USB는 512바이트 전용 SRAM을 공유해 동시에 쓸 수 없다.

## ISR vs 콜백

`USB_LP_CAN1_RX0_IRQHandler`(진짜 ISR) → `HAL_CAN_IRQHandler()` → `HAL_CAN_RxFifo0MsgPendingCallback()`(이 파일): FIFO0에 프레임 도착.

## 관련 예제

없음 (통신 계열: `02_USART`, `04_I2C_Communication`, `05_SPI_Communication`).

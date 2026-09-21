# 03_CAN_Normal_2boards_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**실제 CAN 버스(Normal 모드)**: 보드 2장 + 3.3V CAN 트랜시버. 루프백과 달리 **상대 노드가 ACK 를 줘야** 송신이 성공한다. 오류 카운터(TEC/REC)와 버스오프 복구를 관찰한다.

## 핵심 학습 내용

- **노드 구분**: `build_flags` 에 `-DNODE_ID=0` 또는 `1`(기본 0). 송신 ID = `0x100 + NODE_ID`, 데이터 = 노드번호 + 32비트 카운터 + LED 상태.
- **오류 상태**(`CAN1->ESR`): Error Active → Error Warning(TEC/REC > 96) → Error Passive(> 127) → **Bus-Off**(TEC > 255). 상대가 없으면 ACK 오류로 TEC 가 올라간다.
- **복구**: `AutoBusOff`(`MCR.ABOM`)로 하드웨어 자동 복구, ACK 없이 메일박스가 막히면 `HAL_CAN_AbortTxRequest`로 취소.
- **오류 인터럽트**: `CAN1_SCE_IRQHandler` → `HAL_CAN_ErrorCallback`. 상대 카운터가 건너뛰면 손실을 알린다.

## 배선 / 동작

STM32 PA12(TX)→트랜시버 TXD, PA11(RX)←RXD, 3.3V/GND. CANH-CANH, CANL-CANL 연결, **버스 양 끝 120Ω 종단**. 500kbps. 수신 시 LD2 토글, 1초마다 상태/TEC/REC 를 UART(115200)로 출력.

## ISR vs 콜백

`USB_LP_CAN1_RX0_IRQHandler`(수신), `CAN1_SCE_IRQHandler`(상태/오류) → `HAL_CAN_IRQHandler()` → `HAL_CAN_RxFifo0MsgPendingCallback()`, `HAL_CAN_ErrorCallback()`(이 파일).

## 관련 예제

`01_CAN_Loopback_HAL_c`, `02_CAN_Filter_HAL_c`.

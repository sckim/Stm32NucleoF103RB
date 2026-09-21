# 11_CAN_Communication — CAN

STM32F103의 bxCAN(CAN 2.0A/B)을 다룬다. 번호 체계의 예외로 3자리(100번대)를 사용한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 100 | [100_CAN_Loopback_HAL_c](100_CAN_Loopback_HAL_c/README.md) | HAL | 500kbps 루프백, 수신 FIFO0 인터럽트 |

## 이 그룹에서 배우는 것

- CAN 비트 타이밍(Prescaler, BS1, BS2, SJW)과 샘플 포인트
- 메일박스(송신 3개), 수신 FIFO, 필터 뱅크
- 하드웨어 없이 시험하는 **루프백 모드**

## 참고

Nucleo 보드에는 CAN 트랜시버가 없다. 실제 버스는 3.3V 트랜시버(SN65HVD230 등)와 120Ω 종단이 필요하다.

## 앞으로 추가 예정

`101_CAN_Normal_2boards`(보드 2개 + 트랜시버).

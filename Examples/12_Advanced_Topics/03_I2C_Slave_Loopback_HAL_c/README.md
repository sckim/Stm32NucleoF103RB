# 03_I2C_Slave_Loopback_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

**I2C 슬레이브 모드**를 보드 한 장에서 시험한다. I2C1(마스터)과 I2C2(슬레이브, 주소 0x32)를 점퍼선으로 연결하고, 슬레이브는 16바이트 레지스터 파일을 가진 가상 센서처럼 동작한다.

## 핵심 학습 내용

- **리슨 상태**: `HAL_I2C_EnableListen_IT()`로 자기 주소 호출을 기다린다. 전송이 끝날 때마다(`ListenCplt`) 다시 리슨으로 돌아가야 항상 응답할 수 있다.
- **주소 일치 콜백**: `HAL_I2C_AddrCallback`의 `TransferDirection`은 **마스터 기준**이다. `TRANSMIT`이면 슬레이브가 수신, `RECEIVE`이면 슬레이브가 송신.
- **순차(sequential) API**: `HAL_I2C_Slave_Seq_Receive_IT/Transmit_IT`. 프레임 길이를 고정(쓰기 2바이트, 읽기 4바이트)해 단순하게 처리한다.
- **클럭 스트레칭**: 슬레이브가 SCL 을 잡고 처리 시간을 확보한다. 슬레이브 송신 끝의 마스터 NACK(`AF`)는 정상 종료다.

## 배선 / 동작

PB6(I2C1_SCL) ─ PB10(I2C2_SCL), PB7(I2C1_SDA) ─ PB11(I2C2_SDA). SCL/SDA 에 4.7kΩ 풀업 권장. 마스터가 레지스터에 쓰고 되읽어 일치 여부를 PASS/FAIL 로 UART(115200)에 출력한다.

## ISR vs 콜백

`I2C2_EV_IRQHandler`/`I2C2_ER_IRQHandler` → `HAL_I2C_EV/ER_IRQHandler` → `HAL_I2C_AddrCallback` / `SlaveRxCpltCallback` / `ListenCpltCallback` / `ErrorCallback`(이 파일).

## 관련 예제

`04_I2C_Communication/02_I2C_Scan_HAL_c`(이 슬레이브 주소 0x32 도 스캔에 나타난다), `05_SPI_Communication/04_SPI_Slave_Loopback_HAL_c`.

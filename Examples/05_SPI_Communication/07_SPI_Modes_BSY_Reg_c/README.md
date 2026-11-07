# 07_SPI_Modes_BSY_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「직렬 통신(USART·SPI·I2C)」 SPI 절의 실습이다. 이 그룹의 다른 예제는 모두 모드 0이다. 이 예제는 **모드가 어긋나면 무엇이 깨지는지**와 **전이중 1바이트에서 TXE·RXNE·BSY가 어떤 순서로 바뀌는지**를 레지스터 수준에서 확인한다.

## 핵심 학습 내용

- **CPOL·CPHA** (RM0008 25.3.1, 그림 240): CPOL은 유휴 클럭 레벨, CPHA는 첫 번째(0)·두 번째(1) 에지 중 어느 에지에서 샘플하는지를 정한다. 마스터와 슬레이브가 같은 모드여야 한다.
- **모드 불일치 실험**: 슬레이브 SPI2를 모드 0으로 고정하고 마스터 SPI1을 모드 0~3으로 바꿔, 양쪽이 받은 비트열을 이진수로 출력한다. 비트가 1자리 밀리는지, 전혀 다른 값이 되는지를 관찰한다.
- **"보내야 받는다"**: 마스터는 DR에 쓸 때만 SCK를 낸다. 수신만 할 때도 더미 바이트를 써야 한다. 슬레이브는 클럭을 만들 수 없으므로 응답을 **클럭이 오기 전에** DR에 장전해 둔다.
- **플래그 순서** (25.3.5, 그림 241): DR에 쓰면 데이터가 곧 시프트 레지스터로 옮겨져 TXE가 다시 서고, 8비트가 끝나야 RXNE가 선다. BSY는 전송 중 1이다. SPI를 끄기 전에는 TXE = 1, BSY = 0을 차례로 기다린다.
- **분주 상한**: SPI1은 APB2 64 MHz이므로 ÷2(32 MHz)는 데이터시트 최대 18 Mbit/s를 넘는다. 이 보드에서 SPI1의 실질 상한은 ÷4(16 MHz)이다.
- **소프트웨어 NSS**: 마스터는 SSM = 1, SSI = 1(모드 고장 방지), 슬레이브는 SSM = 1, SSI = 0(항상 선택)으로 두어 NSS 선 없이 시험한다.

## 배선

| 마스터 SPI1 | 슬레이브 SPI2 |
|---|---|
| PA5 (SCK, D13) | PB13 (SCK) |
| PA7 (MOSI, D11) | PB15 (MOSI) |
| PA6 (MISO, D12) | PB14 (MISO) |

`12_Advanced_Topics/04_SPI_Slave_Loopback_c`와 같은 핀이며, NSS 선(PA4–PB12)은 쓰지 않는다. PA5는 LD2와 같은 핀이라 전송 중 LED가 흐릿하게 켜진다.

## 동작

5초마다 두 실험을 반복해 USART2(115200 8N1)로 출력한다. 출력 예:

```
A) slave SPI2 = mode 0 fixed, master SPI1 mode 0..3, SCK 250 kHz
   master mode 0 (CPOL=0,CPHA=0)
     sent 10010110 -> slave got 10010110 | slave sent 01011010 -> master got 01011010
     ...
     match: slave 4/4, master 4/4
   master mode 1 (CPOL=0,CPHA=1)
     ...
B) master full-duplex 1 byte at 250 kHz (8 bits = 32 us), time after DR write
   BSY=1 at 0 us, TXE=1 at 0 us, RXNE=1 at 32 us, BSY=0 at 32 us
```

(모드 1~3의 값은 실측으로 확인한다. 모드마다 두 SPI를 리셋 레지스터로 초기화하므로, 앞 실험에서 어긋난 비트 카운터가 다음 실험에 영향을 주지 않는다.)

## 인터럽트

사용하지 않는다(폴링).

## 관련 예제

`01_SPI_Loopback_c`(HAL 루프백, 분주비별 속도), `02_SPI_DMA_Fullduplex_c`(DMA), `12_Advanced_Topics/04_SPI_Slave_Loopback_c`(HAL 슬레이브, 한 프레임 늦은 응답).

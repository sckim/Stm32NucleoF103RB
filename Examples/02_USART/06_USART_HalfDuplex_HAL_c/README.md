# 06_USART_HalfDuplex_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

USART **단선(half-duplex) 모드**: TX 핀 한 가닥으로 송신과 수신을 번갈아 한다. RS-485, 스마트 서보 버스가 쓰는 방식이다. 보드 한 장에서 시험할 수 있도록 USART1(단선 마스터)과 USART3(가짜 슬레이브)를 한 선에 연결한다.

## 핵심 학습 내용

- **`CR3.HDSEL`(bit3)=1**: RX 를 내부에서 TX 핀에 연결. `HAL_HalfDuplex_Init`이 설정한다.
- **방향 전환**: `HAL_HalfDuplex_EnableTransmitter()` / `EnableReceiver()`. 송신 후에는 `SR.TC`(전송 완료)까지 기다려야 마지막 바이트가 잘리지 않는다.
- **오픈드레인 + 풀업**: 여러 장치가 한 선을 공유하려면 핀을 `AF_OD`로 하고 풀업이 필요하다.
- **프로토콜이 필요**: 마스터가 묻고 슬레이브가 답하는 식으로 "누가 말하는지"를 규칙으로 정한다. 자기 송신이 자기 RX 로 되돌아오는 에코 처리도 함께 본다.

## 배선 / 동작

PA9(USART1_TX) ── PB10(USART3_TX) ── PB11(USART3_RX) 세 핀을 점퍼선으로 한 선에 묶고 4.7kΩ 풀업(3.3V)을 권장한다. 마스터가 `PING`을 보내면 슬레이브가 `PONG`으로 답하고, 결과를 가상 COM(USART2)으로 1초마다 출력한다.

## ISR vs 콜백

인터럽트를 사용하지 않는다(폴링). 실제 RS-485 는 트랜시버의 DE/RE 핀을 방향 전환 시점에 GPIO 로 제어한다.

## 관련 예제

`02_USART_Interrupt_RX_HAL_c`(전이중 수신), `03_USART_DMA_IdleLine_HAL_c`.

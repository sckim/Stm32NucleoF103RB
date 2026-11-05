# 06_USART_HalfDuplex_c

> 계층: **HAL** — 검토 전 (컴파일만 확인)

> CubeMX(`USART_HalfDuplex.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `01_USART_printf`를 기반으로 직접 편집했으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

USART **단선(half-duplex) 모드**: TX 핀 한 가닥으로 송신과 수신을 번갈아 한다. RS-485, 스마트 서보 버스가 쓰는 방식이다. 보드 한 장에서 시험할 수 있도록 USART1(단선 마스터)과 USART3(가짜 슬레이브)를 한 선에 연결한다.

## 핵심 학습 내용

- **`CR3.HDSEL`(bit3)=1**: RX 를 내부에서 TX 핀에 연결. `HAL_HalfDuplex_Init`이 설정한다.
- **방향 전환**: `HAL_HalfDuplex_EnableTransmitter()` / `EnableReceiver()`. 송신 후에는 `SR.TC`(전송 완료)까지 기다려야 마지막 바이트가 잘리지 않는다.
- **오픈드레인 + 풀업**: 여러 장치가 한 선을 공유하려면 핀을 `AF_OD`로 하고 풀업이 필요하다. 코드가 PA9·PB10에 지정한 `GPIO_PULLUP`은 F1의 대체 기능 출력 모드에서는 적용되지 않으므로, 내부 풀업은 입력인 PB11(약 40kΩ) 하나뿐이다. 외부 풀업 저항을 다는 것이 좋다.
- **프로토콜이 필요**: 마스터가 묻고 슬레이브가 답하는 식으로 "누가 말하는지"를 규칙으로 정한다. 자기 송신이 자기 RX 로 되돌아오는 에코 처리도 함께 본다.

## 배선 / 동작

PA9(USART1_TX) ── PB10(USART3_TX) ── PB11(USART3_RX) 세 핀을 점퍼선으로 한 선에 묶고 4.7kΩ 풀업(3.3V)을 권장한다. 마스터가 `PING`을 보내면 슬레이브가 `PONG`으로 답하고, 결과를 가상 COM(USART2)으로 1초마다 출력한다.

**수신은 상대가 보내기 전에 미리 건다.** USART 수신 데이터 레지스터(DR)는 1바이트뿐이라, 송신을 블로킹으로 다 끝낸 뒤에 `HAL_UART_Receive()`로 받기 시작하면 첫 바이트만 남고 나머지는 오버런(`SR.ORE`)으로 사라진다. 그래서 한 주기를 다음 순서로 진행한다.

1. 슬레이브 수신 무장: `HAL_UART_Receive_IT(&huart3, slave_rx, 4)`
2. 마스터 송신: `HAL_HalfDuplex_EnableTransmitter(&huart1)` → `HAL_UART_Transmit(&huart1, "PING")` (TC까지 대기) → 슬레이브 수신 완료 대기
3. 마스터 수신 무장: `HAL_HalfDuplex_EnableReceiver(&huart1)` → `HAL_UART_Receive_IT(&huart1, rsp, 4)`
4. 슬레이브는 자기 송신이 같은 선으로 되돌아오므로 에코 수신도 먼저 건 뒤 `"PONG"` 송신 → 에코와 마스터 수신 완료 대기

수신 완료는 `WaitRx()`가 `RxState`가 READY로 돌아오는지로 확인한다 (시간 초과 20ms면 `HAL_UART_AbortReceive`로 취소). 정상이면 다음과 같이 출력된다.

```
#0  slave got "PING"  master got "PONG"  ok=1 fail=0
```

이전 버전은 수신을 송신 뒤에 시작하는 구조여서, 슬레이브가 첫 바이트(`slave got "P"`)만 받고 매번 실패했다 (소스 분석 결과).

## ISR vs 콜백

송신은 폴링(`HAL_UART_Transmit`), 수신은 인터럽트(`HAL_UART_Receive_IT`, `CR1.RXNEIE=1`)이다. `USART1_IRQHandler` / `USART3_IRQHandler`(`stm32f1xx_it.c`, 진짜 ISR) → `HAL_UART_IRQHandler()`가 RXNE마다 DR을 읽어 버퍼에 쌓고, 지정 바이트 수가 차면 `RXNEIE`를 끄고 `RxState`를 READY로 돌린다. 콜백(`HAL_UART_RxCpltCallback`)은 쓰지 않고 main이 `RxState`를 확인한다. NVIC: `USART1_IRQn`, `USART3_IRQn` (우선순위 0).

실제 RS-485 는 트랜시버의 DE/RE 핀을 방향 전환 시점에 GPIO 로 제어한다.

## 관련 예제

`02_USART_Interrupt`(전이중 수신), `04_USART_DMA_IdleLine_c`.

# 03_DMA_Width_Error_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「DMA」(주변장치 5) 6절의 실습이다. HAL이 감춰 둔 DMA1 레지스터를 직접 설정해 다섯 가지를 숫자로 확인한다. 인터럽트는 쓰지 않고 `DMA1->ISR`을 폴링한다.

## 핵심 학습 내용

- **실험 A — 데이터 폭 변환** (RM0008 13.3.4, 표 76): 메모리→메모리(`CCR.MEM2MEM = 1`, `DIR = 0`이면 `CPAR` 쪽에서 읽어 `CMAR` 쪽에 쓴다)로 `PSIZE` × `MSIZE` 9가지를 돌린다. 좁은 쪽 → 넓은 쪽은 0으로 채우고, 넓은 쪽 → 좁은 쪽은 하위 비트만 남긴다.
- **실험 B — AHB→APB 다리의 복제** (RM0008 13.3.4 후반): DMA가 APB 레지스터에 바이트·하프워드로 쓰면 다리가 32비트 쓰기로 바꾸며 값을 복제한다. `TIM3->ARR`에 0xAB(바이트) → 0xABAB, ARR+1 주소에 0xCD → 0xCDCD가 기대값이다. CPU 바이트 쓰기 결과와 나란히 출력한다(`08_Clock_System/13_BusAccess_Latency_Reg_c`와 비교).
- **실험 C — EN = 1 규칙** (RM0008 13.4.4): 채널이 켜진 동안 `CNDTR`은 읽기 전용(남은 개수)이다. 요청이 없는 채널 4를 켠 채 `CNDTR`에 3을 써도 10이 남는지 본다. `CNDTR = 0`이면 켜도 전송이 없다.
- **실험 D — 전송 오류** (RM0008 13.3.5): 예약 주소를 원본으로 주면 `TEIF`가 서고 하드웨어가 `EN`을 0으로 만든다. 주소 후보 두 개(0x4002_4000, 0x6000_0000)의 결과를 출력한다.
- **실험 E — 버스 공유** (RM0008 13.3, 3.1 BusMatrix): DMA가 SRAM을 채우는 동안 CPU의 SRAM 합계 루프와 레지스터 루프가 각각 얼마나 느려지는지 DWT로 잰다.
- **정오표 반영** (ES096 Rev 15): 2.4.1 — 채널이 동작 중일 때 `CGIFx`로 `GIF`를 지우면 오류 시 자동 비활성화가 실패할 수 있으므로 `IFCR`에는 `CTEIF`·`CHTIF`·`CTCIF`만 쓴다. 2.4.2 — DMA 레지스터는 32비트 정렬 접근만 쓴다(RM0008 13.4의 "바이트·하프워드 접근 가능" 문장은 틀렸다).

## 레지스터

| 레지스터 | 비트 | 의미 |
|---|---|---|
| `DMA1_Channelx->CCR` | `EN`(0) `TCIE`(1) `HTIE`(2) `TEIE`(3) `DIR`(4) `CIRC`(5) `PINC`(6) `MINC`(7) `PSIZE`(9:8) `MSIZE`(11:10) `PL`(13:12) `MEM2MEM`(14) | 채널 설정. `EN = 0`일 때 바꾼다 |
| `DMA1_Channelx->CNDTR` | `NDT`(15:0) | 남은 전송 개수. `EN = 1`이면 읽기 전용 |
| `DMA1_Channelx->CPAR/CMAR` | 32비트 주소 | 켜진 동안 쓰면 안 된다 |
| `DMA1->ISR / IFCR` | 채널마다 `GIF` `TCIF` `HTIF` `TEIF` 4비트 | 플래그 읽기 / 1을 써서 지우기 |
| `RCC->AHBENR` | `DMA1EN`(0) | DMA1 클럭 |

## 동작

배선은 필요 없다. 5초마다 다섯 실험을 반복해 USART2(ST-LINK 가상 COM, 115200 8N1)로 출력하고, LD2를 토글한다.

실험 A의 기대 출력(표 76과 같은 규칙으로 계산한 값):

```
   P 8 -> M 8 : 00 01 02 03 EE EE EE EE EE EE EE EE EE EE EE EE
   P 8 -> M16 : 00 00 01 00 02 00 03 00 EE EE EE EE EE EE EE EE
   P 8 -> M32 : 00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00
   P16 -> M 8 : 00 02 04 06 EE EE EE EE EE EE EE EE EE EE EE EE
   P16 -> M16 : 00 01 02 03 04 05 06 07 EE EE EE EE EE EE EE EE
   P16 -> M32 : 00 01 00 00 02 03 00 00 04 05 00 00 06 07 00 00
   P32 -> M 8 : 00 04 08 0C EE EE EE EE EE EE EE EE EE EE EE EE
   P32 -> M16 : 00 01 04 05 08 09 0C 0D EE EE EE EE EE EE EE EE
   P32 -> M32 : 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
```

RM0008 Rev 21 표 76에는 오타가 있다. 8→16·8→32 행의 3·4번째 줄이 `B3`·`B4`로 적혀 있지만 `B2`·`B3`이 맞고, 32→16 행의 `B1B0[7:0]`은 `[15:0]`이 맞다. 실측 덤프로 확인한다.

실험 B~E는 실측값을 보고 판단하는 질문이다. 특히 D(어느 주소가 오류를 내는가)와 E(CPU가 얼마나 느려지는가)는 매뉴얼에 숫자가 없다.

## 생각해 볼 것

- 실험 E에서 레지스터 루프는 왜 거의 느려지지 않는가? 명령어는 Flash에서 ICode로, 데이터는 SRAM에서 System 버스로 온다는 점을 메모리 맵 장 3절의 버스 행렬 그림에 대 보자.
- 실험 C에서 채널 4를 고른 이유는 무엇인가? 이 채널에 연결된 요청(USART1_TX, SPI2_RX, I2C2_TX, TIM1_CH4·TRIG·COM, TIM4_CH2)을 모두 꺼 두었기 때문이다. 하나라도 켜져 있으면 실험이 달라진다.

## ISR vs 콜백

인터럽트를 사용하지 않는다. 진짜 ISR도 HAL 콜백도 없다.

## 관련 예제

`01_DMA_MemToMem`(HAL 메모리→메모리), `02_DMA_Circular_GPIO`(타이머 요청 + 원형 모드), `12_Advanced_Topics/06_DMA_Priority_c`(채널 중재), `08_Clock_System/13_BusAccess_Latency_Reg_c`(CPU 쪽 버스 접근 비용).

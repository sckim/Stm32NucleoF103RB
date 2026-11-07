# 09_USART_ErrorFlags_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「직렬 통신(USART·SPI·I2C)」 USART 절의 실습이다. HAL이 감추는 USART 상태 플래그의 의미를 직접 재 본다.
USART1이 보내고 USART3이 받으며, 결과는 USART2(가상 COM)로 출력한다.

## 핵심 학습 내용

- **TXE와 TC는 다르다** (RM0008 27.3.2, 그림 282): TXE는 "TDR이 비어 다음 바이트를 써도 된다", TC는 "시프트 레지스터까지 비어 선 위의 마지막 비트가 끝났다"이다. 반이중 방향 전환, USART 끄기, 저전력 진입 전에는 TC를 기다려야 한다.
- **BRR = 반올림(fCK / baud)**: BRR은 16 × USARTDIV이므로 정수부 12비트와 소수부 4비트를 따로 계산하지 않아도 된다. 소수부 자리올림(예: USARTDIV 50.99 → 51.0)도 자동으로 처리된다.
- **수신 허용 오차** (27.3.5, 표 194): 16배 오버샘플링 수신기는 비트 중앙(8·9·10번째 샘플)을 다수결로 읽는다. M = 0이고 BRR 소수부 ≠ 0이면 허용 오차는 3.33 %(NE를 오류로 볼 때) ~ 3.88 %(NE 무시)이다. 송신기 보레이트를 0 ~ ±5 %로 틀어 정상·FE·NE 수가 어디서 바뀌는지 본다.
- **오류 플래그 해제 순서**: FE·NE·ORE는 "SR 읽기 → DR 읽기"로 지워진다. 오류 비트는 RXNE와 함께 서므로 SR을 먼저 읽어 두지 않으면 어느 바이트에 오류가 있었는지 알 수 없다.
- **오버런** (27.3.3): RXNE가 1인 동안 다음 바이트가 끝나면 ORE가 선다. RDR에는 먼저 받은 바이트가 그대로 남고, 그 뒤 바이트는 시프트 레지스터에서 덮어써져 사라진다.

## 배선

`PA9 (USART1_TX, Arduino D8) ── 점퍼 ── PB11 (USART3_RX, Morpho CN10-18)`

PB11은 내부 풀업 입력으로 설정했으므로 점퍼가 빠져도 선이 유휴(High)로 보인다.

## 레지스터

| 레지스터 | 값 | 의미 |
|---|---|---|
| `USART3->BRR` | 0x116 (278) | 32 MHz / 115200 = 277.8 → USARTDIV 17.375 |
| `USART1->BRR` | 0x22C (556) 기준 | 64 MHz / 115200 = 555.6 → USARTDIV 34.75. 실험 B에서 바꾼다 |
| `USARTx->CR1` | UE, TE 또는 RE | 송신기는 TE, 수신기는 RE만 켠다 |

## 동작

5초마다 세 실험을 반복해 USART2(115200 8N1)로 출력한다. 출력 예:

```
A) USART1 115200, write 0x55 then 0xAA (1 frame = 86.8 us)
   TXE #1 after 0 us | TXE #2 after 86 us | TC after 173 us   (expect ~0 / ~87 / ~174)
B) RX USART3 fixed 115200 (BRR 0x116). TX USART1 baud x (1+e), 200 bytes of 0x55/0xF0
   e(%)   TX BRR  actual(%)   ok  wrong  FE  NE  missing
   +0.00  0x22C   -0.08   200     0   0   0     0
   +3.00  0x21C   +2.92   ...
C) sent '1'..'5' without reading USART3
   SR before read: RXNE=1 ORE=1 | DR = '1' | after SR->DR read: RXNE=0 ORE=0
```

(값은 예시이다. 실험 B에서 정상 수가 0으로 떨어지는 오차와 NE가 먼저 나타나는 구간이 표 194의 3.33 % · 3.88 %와 맞는지 확인하는 것이 이 실험의 질문이다. 동기가 깨지면 이후 바이트의 기대값 비교가 어긋나 `wrong`이 함께 늘어난다.)

## 인터럽트

사용하지 않는다(모두 폴링).

## 관련 예제

`01_USART_printf`(HAL 송신), `02_USART_Interrupt`(RXNE 인터럽트), `06_USART_HalfDuplex_c`(TC를 기다리는 방향 전환), `08_USART_printf_Dual`(USART1 파형 관찰), `08_Clock_System/10_DWT_Profiling_Reg_c`(DWT).

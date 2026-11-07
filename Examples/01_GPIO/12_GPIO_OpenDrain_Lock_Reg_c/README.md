# 12_GPIO_OpenDrain_Lock_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「GPIO·AFIO」 6.1절(네 가지 경로)과 6.5절(리셋 상태와 설정 잠금)의 실습이다. 오픈드레인 출력 두 개로 **와이어드 AND**를 만들고, `LCKR`로 핀 설정을 **잠근다**.

## 핵심 학습 내용

- **오픈드레인**: 0이면 N-MOS가 선을 Low로 당기고, 1이면 고임피던스로 놓는다(P-MOS는 항상 꺼짐). 오픈드레인 출력 여러 개를 한 선에 묶고 풀업 하나를 달면 선 = 모든 출력의 AND가 된다. I2C 버스의 기본 구조이다.
- **F1은 출력 모드에서 내부 풀업이 꺼진다**(RM0008 9.1.8). 그래서 오픈드레인 핀 자신의 풀업은 쓸 수 없다. 이 예제는 같은 노드에 묶은 PA4를 **풀업 입력**으로 두어 그 약한 풀업(30~50 kΩ)을 노드 풀업으로 쓴다.
- **오픈드레인에서 IDR은 선의 실제 레벨**이다. PA0에 1을 써도 PA1이 0이면 PA0의 IDR은 0이다.
- **LCKR 잠금 순서**(RM0008 9.2.7): `LCKK=1|LCK` 쓰기 → `LCKK=0|LCK` 쓰기 → `LCKK=1|LCK` 쓰기 → 읽기(LCKK=0) → 읽기(LCKK=1). 순서 중 LCK 값이 바뀌면 잠금이 취소된다. 잠긴 핀의 CRL/CRH 니블은 **다음 리셋까지** 바뀌지 않는다.

## 레지스터

| 레지스터 | 값 | 의미 |
|---|---|---|
| `GPIOA->CRL` PA0·PA1 | 0x6 | 범용 오픈드레인 2 MHz (CNF 01, MODE 10) |
| `GPIOA->CRL` PA4 | 0x8 + `ODR4 = 1` | 풀업 입력 |
| `GPIOA->LCKR` | `LCK0`, `LCKK` | PA0 설정 잠금 |

## 배선

`PA0(A0) ── PA1(A1) ── PA4(A2)` 세 핀을 점퍼로 한 점에 묶는다. 배선이 없으면 실험 C의 IDR 값이 정해지지 않는다(그것도 관찰 대상이다). 실험 D는 배선과 무관하다.

## 동작

USART2(115200 8N1) 출력 예(배선한 경우의 기대값):

```
12_GPIO_OpenDrain_Lock_Reg_c
C) wired-AND  (jumper A0-A1-A2; PA4 pull-up is the only pull-up on the node)
   ODR0 ODR1 | IDR0 IDR1 IDR4(node) | expect node
     0    0  |   0    0      0        |     0
     0    1  |   0    0      0        |     0
     1    0  |   0    0      0        |     0
     1    1  |   1    1      1        |     1
D) LCKR lock of PA0 (irreversible until reset)
   lock sequence -> LCKK = 1  LCKR = 0x10001
   CRL nibble PA0 (locked)  : 0x6 -> write 0x2 -> 0x6
   CRL nibble PA1 (unlocked): 0x6 -> write 0x2 -> 0x2
   PA0 ODR after BR0 / BS0  : ? / ?   (does the lock freeze ODR too?)
   (reset the board to unlock)
```

실험 D의 마지막 줄은 RM0008이 "각 잠금 비트는 CRL/CRH의 4비트를 고정한다"고 적은 것을 실측으로 확인하는 질문이다. 잠금이 출력 데이터까지 막는지는 보드에서 실행해 확인한다. 실험 D는 리셋 직후 한 번만 실행되고, 이후에는 실험 C만 3초마다 반복한다.

## 생각해 볼 것

- PA4의 풀업(약 40 kΩ)과 노드 용량으로 상승 시간을 어림해 보자. 코드의 20 µs 대기는 충분한가?
- 오픈드레인 대신 두 핀을 푸시풀로 두고 서로 다른 값을 쓰면 어떻게 되는가? (단락 전류가 흐르므로 실제로 해 보지는 않는다.)

## 인터럽트

사용하지 않는다.

## 관련 예제

`04_I2C_Communication/01_I2C_Scan`(AF 오픈드레인 I2C), `11_GPIO_BSRR_Race_Speed_Reg_c`(푸시풀 출력과 BSRR).

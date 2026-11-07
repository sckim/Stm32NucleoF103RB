# 11_GPIO_BSRR_Race_Speed_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「GPIO·AFIO」 6.2절(ODR과 BSRR)과 4.2·6.1절(출력 속도)의 실습이다. `04_BlinkReg`가 왜 `ODR`이 아니라 `BSRR`로 핀을 바꾸는지를 **경쟁을 직접 일으켜** 확인하고, 출력 속도 설정(MODE)이 무엇을 바꾸는지 본다.

## 핵심 학습 내용

- **읽기-수정-쓰기 경쟁**: `GPIOB->ODR |= ...`는 읽기 → 수정 → 쓰기 세 단계이다. 그 사이에 인터럽트가 같은 포트의 다른 비트를 바꾸면, main이 옛 값을 다시 써서 그 변경을 지운다.
- **BSRR**: 아래 16비트(BS)에 1을 쓴 비트는 set, 위 16비트(BR)에 1을 쓴 비트는 reset, 0인 비트는 그대로이다. 읽기가 없으므로 경쟁이 원리적으로 없다(RM0008 9.1.2, 9.2.5). BS와 BR이 함께 1이면 BS 우선.
- **실험 A**: main이 PB0을, TIM2 갱신 ISR(100 kHz)이 PB1을 토글한다. 두 쪽 모두 ODR 읽기-수정-쓰기를 쓰면 PB1 갱신이 유실되는 횟수를 세고, 두 쪽 모두 BSRR로 바꾸면 0이 되는 것을 확인한다. 경쟁 창을 넓히려고 main의 읽기와 쓰기 사이에 짧은 지연(`WINDOW_LOOPS`)을 넣었다.
- **실험 B**: PA8(D7)의 `CRH.MODE8`을 10(2 MHz) → 01(10 MHz) → 11(50 MHz)로 바꾸며 같은 BSRR 루프로 토글한다. 토글 속도는 같고, 달라지는 것은 에지의 기울기이다(데이터시트 표 38: 최대 상승·하강 시간 125 ns / 25 ns / 5 ns).

## 레지스터

| 레지스터 | 값 | 의미 |
|---|---|---|
| `GPIOB->CRL[7:0]` | 0x33 | PB0·PB1 범용 푸시풀 50 MHz |
| `GPIOA->CRH[3:0]` | 0x2 / 0x1 / 0x3 | PA8 범용 푸시풀 2 / 10 / 50 MHz |
| `GPIOB->BSRR` | `BS0`·`BR0`, `BS1`·`BR1` | 원자적 set/reset |
| `TIM2->PSC` / `ARR` | 63 / 9 | 64 MHz / 64 / 10 = 100 kHz 갱신 인터럽트 |

## 배선 / 관찰

배선 없이 실험 A가 동작한다. 실험 B는 AD2·오실로스코프로 **PA8(D7)**의 에지를 본다(같은 프로브와 같은 부하로 비교).

## 동작

USART2(ST-LINK 가상 COM, 115200 8N1) 출력 예:

```
11_GPIO_BSRR_Race_Speed_Reg_c  (SYSCLK 64 MHz, ISR 100 kHz on PB1, main toggles PB0)
[round 1]
  A) ODR read-modify-write : lost PB1 updates = 4123 / ISR runs 48210
     BSRR atomic write     : lost PB1 updates = 0 / ISR runs 30115   (expect 0)
  B) output speed setting
  PA8(D7) MODE=10 (2 MHz)  : toggle rate ~5333 kHz (same for every MODE; compare edges on a scope)
  PA8(D7) MODE=01 (10 MHz) : toggle rate ~5333 kHz ...
  PA8(D7) MODE=11 (50 MHz) : toggle rate ~5333 kHz ...
```

(숫자는 예시이다. ODR 방식의 유실 횟수는 `WINDOW_LOOPS`와 ISR 주기에 따라 달라진다. 2 MHz 설정에서 수 MHz 토글을 하면 파형이 사각파가 되지 못하는 것도 관찰 대상이다.)

## 생각해 볼 것

- `WINDOW_LOOPS`를 0으로 줄이면 유실이 사라지는가? 창이 짧아지면 드물어질 뿐 없어지지는 않는다.
- 같은 문제를 비트 밴딩(`09_BitBandingReg`)이나 인터럽트 차단으로 막을 수도 있다. 이식성과 지연을 비교해 보자.

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR, 이 파일)가 `SR.UIF`를 지우고 PB1만 바꾼다. HAL 콜백은 없다. 출력은 main 루프가 한다.

## 관련 예제

`04_BlinkReg`(BSRR 출력), `09_BitBandingReg`(비트 밴딩), `08_Clock_System/10_DWT_Profiling_Reg_c`(DWT 측정).

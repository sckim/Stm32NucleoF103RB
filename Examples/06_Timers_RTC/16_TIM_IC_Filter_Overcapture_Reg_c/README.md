# 16_TIM_IC_Filter_Overcapture_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전

교안 「타이머(TIM)」 6.3절(입력 캡처)과 8절(설계상의 한계)의 실습이다.
입력 캡처 경로의 **디지털 필터**(`CCMR1.IC1F`)가 짧은 글리치를 어떻게 걸러 내는지, 그리고 캡처 값을 늦게 읽으면 **오버캡처**(`SR.CC1OF`)로 값이 사라지는 과정을 숫자로 확인한다.

## 핵심 학습 내용

- **입력 경로 = 필터 → 에지 검출 → 분주 → 래치**. 필터는 `f_SAMPLING`으로 N번 연속 같은 레벨을 봐야 전이로 인정한다(RM0008 15.4.7, IC1F).
  - `IC1F = 0000`: 필터 없음
  - `IC1F = 0011`: `f_SAMPLING = f_CK_INT`(64 MHz), N = 8 → 약 125 ns
  - `IC1F = 0101`: `f_SAMPLING = f_DTS/2`(32 MHz, CKD = 00), N = 8 → 약 250 ns
- **필터의 대가는 지연**: 모든 에지가 같은 만큼 늦어지므로 주기 측정에는 영향이 없지만, 절대 시점이 중요한 응용에서는 따져야 한다.
- **오버캡처**: CCR1은 값 하나만 담는다. `CC1IF = 1`인 채로 다시 캡처되면 이전 값이 덮어써지고 `CC1OF`가 선다. `CC1IF`는 CCR1을 읽으면 지워지고, `CC1OF`는 0을 써야 지워진다.
- **채널 설정 순서**: `CC1S`는 `CC1E = 0`일 때만 쓸 수 있다. 필터를 바꿀 때도 `CC1E`를 끄고 바꾼 뒤 다시 켠다.

## 테스트 신호와 배선

`PA6(D12) ── 점퍼 ── PA0(A0)` (06·07 예제와 같은 배선)

TIM3_CH1(PA6)이 1 kHz PWM을 낸다(64 MHz, PSC = 0, ARR = 63999). 펄스 폭 `CCR1`을 32000(500 µs) → 32(500 ns) → 12(187.5 ns) → 4(62.5 ns)로 바꿔 가며 짧은 펄스를 "글리치"로 쓴다.
TIM2_CH1(PA0)이 상승 에지를 캡처한다(1 MHz 카운트).

## 동작

USART2(115200 8N1) 출력 예:

```
A) captures per second (input: 1 kHz pulses)
   pulse width | IC1F=0000 | IC1F=0011(~125ns) | IC1F=0101(~250ns)
   500 us      |      1000 |              1000 |              1000
   500 ns      |      1000 |              1000 |              1000
   187.5 ns    |      1000 |              1000 |                 0
   62.5 ns     |      1000 |                 0 |                 0
B) overcapture with polling (interrupt off, 500 us pulses at 1 kHz)
   poll every 10000 us : captures read =  100 , overcapture (CC1OF) =  100
   poll every   500 us : captures read = 1000 , overcapture (CC1OF) =    0
```

(값은 기대치이다. 필터 길이와 펄스 폭이 비슷한 칸은 재동기화 지연으로 결과가 갈릴 수 있고, 62.5 ns 펄스는 핀 출력단·점퍼의 대역폭 때문에 필터 없이도 일부 사라질 수 있다. **표가 기대와 다른 칸이 있으면 왜 그런지 설명해 보는 것이 이 실험의 질문이다.**)

## ISR vs 콜백

`TIM2_IRQHandler`(진짜 ISR, 이 파일)는 실험 A에서만 쓴다. CCR1을 읽어 CC1IF를 지우고 횟수만 센다. 실험 B는 인터럽트를 끄고 main 루프가 SR을 폴링한다. HAL 콜백은 없다.

## 관련 예제

`06_TIM_InputCapture_c`(HAL 입력 캡처로 주파수 측정), `07_TIM_PWMInput_c`(PWM 입력 모드), `09_TIM_Encoder_c`(입력 필터로 접점 튐 제거).

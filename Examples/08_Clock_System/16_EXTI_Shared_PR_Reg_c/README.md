# 16_EXTI_Shared_PR_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「인터럽트 체계(NVIC·EXTI·SysTick)」 6.6절(EXTI)의 실습이다. `01_GPIO/06_ExtInt`·`08_ExternalInt`(HAL)가 숨기는 EXTI 경로를 레지스터로 직접 만들고, **공유 벡터 판별**, **PR 지우기 함정**, **접점 튐**을 숫자로 본다.

## 경로

`PC13(B1)` → `AFIO_EXTICR4.EXTI13 = 0010`(포트 C) → EXTI 라인 13 → `FTSR13`(하강 에지) → `PR13` → `IMR13` → NVIC `EXTI15_10_IRQn`(IRQ 40)

라인 10은 핀 없이 `SWIER10`(소프트웨어 트리거)으로만 쓴다. 라인 10~15는 벡터 하나를 공유하므로 ISR이 `EXTI->PR`을 읽어 라인을 판별한다.

## 핵심 학습 내용

- **AFIO 클럭 먼저**: `RCC->APB2ENR.AFIOEN`을 켜야 `AFIO->EXTICR`에 쓸 수 있다. 같은 번호의 라인은 포트 하나만 고를 수 있다(PA13과 PC13을 동시에 EXTI13으로 쓸 수 없다).
- **실험 A — 공유 벡터**: 라인 10과 13이 함께 대기하면 ISR은 **한 번** 들어와 두 라인을 모두 처리한다. `IMR`을 끈 라인은 NVIC 요청을 내지 않는다.
- **실험 B — PR을 지우지 않으면**: PR 비트가 1인 동안 EXTI는 NVIC에 요청을 계속 내므로 ISR이 끝없이 다시 들어온다(안전장치로 100회에서 멈춘다).
- **실험 C — 언제 지우나**: 맨 앞 / 맨 끝 / 맨 끝 + 되읽기 세 방식으로 200회씩 트리거해 "추가 진입" 수를 센다. 맨 끝에서 지우면 쓰기가 반영되기 전에 예외 복귀가 끝나 한 번 더 들어올 수 있다는 보고가 있다. **이 예제의 실측으로 이 보드에서 실제로 그런지 확인한다.** HAL의 `HAL_GPIO_EXTI_IRQHandler()`는 콜백을 부르기 **전에** PR을 지운다.
- **실험 D — 접점 튐**: B1을 한 번 누를 때 하강 에지가 몇 번 잡히는지 50 ms 창으로 센다. `01_GPIO/06_ExtInt`에 디바운스가 없어 여러 번 토글될 수 있는 이유이다.

## 레지스터

| 레지스터 | 값 | 의미 |
|---|---|---|
| `AFIO->EXTICR[3]` [7:4] | 0010 | EXTI13 ← PC13 |
| `EXTI->FTSR` | bit13 | 하강 에지 |
| `EXTI->IMR` | bit10, bit13 | 인터럽트 마스크 해제 |
| `EXTI->SWIER` | bit10 | 소프트웨어 트리거(PR10을 세움) |
| `EXTI->PR` | rc_w1 | 1을 써서 지움 |
| NVIC | `EXTI15_10_IRQn`, 우선순위 2 | 공유 벡터 |

## 동작

배선 없음. 3초마다 A~C를 반복하고, B1을 누를 때마다 D를 출력한다(USART2 115200 8N1).

```
A) SWIER10+SWIER13 together : ISR entries = 1 , line10 handled = 1   (expect 1 entry; IMR13 was off so PR13 alone did not request)
   with IMR13 on            : ISR entries = 1 , line10 = 1 , line13 = 1   (expect 1 entry handling both lines)
B) PR10 never cleared       : ISR entries for 1 trigger = 100   (stopped by guard at 100)
C) when PR is cleared inside the ISR
   clear first           : 200 triggers -> ISR entries = 200 , extra = 0
   clear last            : 200 triggers -> ISR entries = ... , extra = ...
   clear last + readback : 200 triggers -> ISR entries = 200 , extra = 0
D) B1 press: falling edges in 50 ms = 3  (bounce!)
```

(C의 "clear last" 결과와 D의 에지 수는 보드와 버튼에 따라 다르다. **실측값을 기록해 두는 것이 이 실험의 목적이다.**)

## ISR vs 콜백

`EXTI15_10_IRQHandler`(진짜 ISR, 이 파일)가 `EXTI->PR`을 읽어 라인을 판별하고, 모드에 따라 지우는 시점을 바꾼다. HAL 콜백은 없다. HAL 예제에서는 `EXTI15_10_IRQHandler` → `HAL_GPIO_EXTI_IRQHandler()`(PR 지움) → `HAL_GPIO_EXTI_Callback()` 순서이다.

## 관련 예제

`01_GPIO/06_ExtInt`(HAL, 단일 라인), `01_GPIO/08_ExternalInt`(HAL, 두 벡터), `11_NonBlocking_StateMachine_c`(디바운스), `15_NVIC_Pending_TailChain_Reg_c`(NVIC 대기·우선순위).

# 15_NVIC_Pending_TailChain_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「인터럽트 체계(NVIC·EXTI·SysTick)」 4~6절의 실습이다.
주변장치 없이 `NVIC->ISPR`에 직접 써서 IRQ를 **대기(pending)** 상태로 만들고, ISR 진입·탈출 순서와 사이클을 기록해 NVIC의 판정 규칙을 눈으로 확인한다.

## 핵심 학습 내용

- **NVIC는 요청의 출처를 묻지 않는다**: IRQ7(EXTI1)·IRQ8(EXTI2)·IRQ9(EXTI3)은 EXTI 설정 없이 소프트웨어 대기만으로 실행된다.
- **실험 1 — 진입 지연**: ISPR에 쓴 순간부터 ISR 첫 줄까지의 사이클(DWT). Cortex-M3 최소값은 무대기 메모리 기준 12사이클이고, 64 MHz(Flash 2 웨이트)에서는 더 길게 나온다.
- **실험 2 — 같은 우선순위**: 9→8→7 순으로 대기시켜도 **IRQ 번호가 작은 7부터** 실행되고, 스택 복원 없이 다음 ISR로 넘어간다(**테일 체이닝**). X→E 간격 사이클을 진입 지연과 비교한다.
- **실험 3 — 다른 우선순위**: 번호와 무관하게 **우선순위 값이 작은 것**부터.
- **실험 4 — 선점과 중첩**: IRQ7(우선순위 3) ISR 안에서 IRQ9(1)을 대기시키면 즉시 끼어들고(중첩), IRQ8(3)은 같은 우선순위라 기다렸다가 테일 체이닝으로 실행된다.
- **실험 5 — BASEPRI**: `BASEPRI = 0x20`이면 우선순위 값 2 이상이 막혀 **대기 상태로 남는다**(ISPR 비트로 확인). 풀면 바로 실행된다.
- **실험 6 — 서브 우선순위**: `PRIGROUP = 5`(선점 2비트 + 서브 2비트)에서 선점 우선순위가 같으면 서브가 높아도 선점하지 못한다.

## 레지스터

| 레지스터 | 쓰임 |
|---|---|
| `NVIC->ISER[0]` | IRQ7~9 허용 |
| `NVIC->ISPR[0]` / `ICPR[0]` | 대기 설정 / 해제 (1을 쓴 비트만) |
| `NVIC->IP[n]` | 우선순위. 상위 4비트만 구현(`NVIC_SetPriority(irq, p)` → `p << 4`) |
| `SCB->AIRCR.PRIGROUP` | 선점/서브 분할. 쓰기에는 `VECTKEY = 0x5FA` 필요(`NVIC_SetPriorityGrouping`) |
| `PRIMASK`, `BASEPRI` | 전체 차단 / 우선순위 문턱 차단 |
| `DWT->CYCCNT` | 64 MHz 사이클 시각 |

## 동작

배선 없음. 5초마다 여섯 실험을 반복하고 USART2(115200 8N1)로 출력한다.

```
1) ISPR write -> first ISR line [cycles @64MHz]: min 19 , max 21   (zero-wait Cortex-M3 minimum is 12; Flash 2WS adds more)
2) same priority 2/2/2, pended 9,8,7 under PRIMASK
   order : 7E 7X 8E 8X 9E 9X
   expect: 7E 7X 8E 8X 9E 9X (lowest IRQ number first, tail-chained)
   X->E gap [cycles]: 9 9
4) in IRQ7 (prio 3): pend IRQ9 (prio 1), then IRQ8 (prio 3)
   order : 7E 9E 9X 7m 7X 8E 8X
...
```

(사이클 값은 예시이다. 실제 값은 보드에서 확인한다. 실험 1의 값에는 ISPR 쓰기 자체와 DWT 읽기 시간도 들어 있다.)

## ISR vs 콜백

`EXTI1_IRQHandler` / `EXTI2_IRQHandler` / `EXTI3_IRQHandler`(진짜 ISR, 이 파일). 기록만 하고, 시나리오에 따라 다른 IRQ를 대기시킨다. HAL 콜백은 없다.

## 관련 예제

`06_NVIC_Priority_c`(HAL·타이머로 본 선점과 지터), `03_ExceptionModel_VectorTable_Reg_c`(스택 프레임, EXC_RETURN), `02_USART/07_USART_Stack`(PRIMASK 임계 구역), `12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c`(대기 비트로 PendSV를 일으키는 응용).

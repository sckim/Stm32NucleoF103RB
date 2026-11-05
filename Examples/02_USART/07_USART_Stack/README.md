# 07_USART_Stack

> 계층: **HAL + CMSIS 내장 함수** (STM32CubeMX 생성 프로젝트)

> CubeMX(`USART_Stack.ioc`) 생성 코드 기반. 사용자 코드는 `/* USER CODE BEGIN ... */` 영역 안에만 있다. `.ioc`는 `03_USART_RingBuffer`를 복사해 프로젝트 이름만 바꿨으므로 CubeMX에서 열어 한 번 Generate 해 확인할 것.

교안: [스택 교안.html](스택%20교안.html) (짝 교안: [03_USART_RingBuffer/UART 링 버퍼 교안.html](../03_USART_RingBuffer/UART%20링%20버퍼%20교안.html))

링 버퍼(FIFO)로 받은 글자를 **스택(LIFO)으로 괄호 검사**하고, ISR과 main이 같은 스택을 공유할 때 생기는 **경쟁 상태(race condition)** 를 직접 재현한다.

## 핵심 학습 내용

- **스택 규칙**
  - 배열 + `top` 하나. `top` = 다음에 쓸 빈 칸 = 저장된 개수.
  - empty = `top == 0`, full = `top == size`. 링 버퍼와 달리 되감기가 없어 **N칸을 모두** 쓰고, 크기가 2의 거듭제곱일 필요도 없다.
  - push: 검사 → 쓰기 → `top++`, pop: 검사 → `top--` → 읽기. overflow / underflow 검사는 함수 안에서.
- **괄호 검사기**: 여는 괄호 push, 닫는 괄호 pop 후 짝 비교. 실패는 mismatch / underflow / unclosed / overflow 네 가지.
- **FIFO와 LIFO의 역할 분담**: ISR은 링 버퍼에 push만(lock-free), 스택은 main 안에서만 → 임계 구역이 필요 없다.
- **경쟁 상태 실험(B1)**: 스택은 push와 pop이 같은 `top`을 고치므로 ISR과 공유하면 lock-free가 성립하지 않는다. `top++`/`top--`는 `LDR → ADDS/SUBS → STR` 세 명령이라 그 사이에 ISR이 끼어들면 항목이 사라진다. PRIMASK 저장 → `__disable_irq()` → `__set_PRIMASK()` 복원으로 보호한다.

## 동작 (터미널 115200 8N1)

| 입력 | 출력 예 |
|---|---|
| `{a[b(c)d]e}` + Enter | `OK (max_depth=3/8)` |
| `([)]` | `mismatch at col 3: '[' closed by ')'` |
| `(a))` | `underflow at col 4: no open bracket to close` |
| `((a)` | `unclosed: 1 open bracket(s) left, top='('` |
| `(((((((((x)))))))))` (9단) | `overflow at col 9: nesting deeper than 8` |
| B1 누름 | `unprotected : isr_push=500, top=…, lost=…` / `PRIMASK : … lost=0` |

- 괄호 검사 스택 크기 `BR_STK_SIZE`는 교안 시뮬레이터 기본값과 같은 8이다.
- B1 실험은 각 500 ms 동안 SysTick ISR(1 ms)이 공유 스택에 push하고, main은 같은 스택에 push/pop을 짝지어 반복한다. main의 순증감은 0이므로 정상이라면 `top == isr_push`이고, `lost = isr_push − top`이 사라진 항목 수이다. 보호 없음에서는 lost > 0, PRIMASK 보호에서는 lost = 0이 기대값이다. lost의 정확한 크기는 최적화 수준과 루프 구성에 따라 달라진다.

## ISR vs 콜백

- `USART2_IRQHandler`(진짜 ISR, `stm32f1xx_it.c`) → `HAL_UART_IRQHandler()` → `HAL_UART_RxCpltCallback()`(HAL 콜백, ISR 문맥: 링 버퍼 push)
- `SysTick_Handler`(진짜 ISR, `stm32f1xx_it.c`) → `HAL_IncTick()` 다음 `USER CODE SysTick_IRQn 1` 영역에서 `Stack_RaceTick()`(사용자 함수, ISR 문맥: 공유 스택 push)

관련 하드웨어: `USART2->CR1.RXNEIE`, `SR.RXNE`, `DR`, SysTick, PRIMASK 특수 레지스터.

## 관련 예제

- `03_USART_RingBuffer`: 이 예제의 수신부. FIFO, lock-free 생산자-소비자.
- `05_UART_CLI_c`: 줄 편집의 백스페이스(라인 버퍼 `len--`)도 스택과 같은 원리.
- 하드웨어 스택(MSP): `08_Clock_System/03_ExceptionModel_VectorTable_Reg_c`(예외 자동 저장, EXC_RETURN), `12_Advanced_Topics/10_StackWatermark_Reg_c`(스택 페인팅, 재귀 실험), `08_Clock_System/08_HardFault_Diagnosis_Reg_c`(폴트 시 스택 프레임 분석).

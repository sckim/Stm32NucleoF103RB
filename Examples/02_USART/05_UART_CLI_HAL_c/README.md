# 05_UART_CLI_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

UART 명령줄 인터페이스(CLI): 한 줄 입력 편집 + 명령어 테이블.

## 핵심 학습 내용

- **3단계 설계**
  1. 수신: 인터럽트가 링 버퍼(`04_USART_RingBuffer_HAL_c`과 같은 방식)에 1바이트씩 넣는다.
  2. 줄 조립: main이 한 글자씩 꺼내 라인 버퍼에 쌓고 에코/Backspace(0x08/0x7F) 처리, Enter(CR/LF)에서 한 줄 완성.
  3. 파싱: 공백으로 토큰 분리(`strtok`) → 명령 테이블(이름, 핸들러)에서 검색 → 핸들러 호출.
- **테이블 기반 디스패치**: 명령을 추가하려면 핸들러 함수를 만들고 `cmd_table`에 한 줄 추가하면 된다 (`typedef void (*cmd_fn)(int argc, char **argv)`).
- 디버깅/설정용 인터페이스로서의 CLI 활용.

## 명령

| 명령 | 동작 |
|---|---|
| `help` | 명령 목록 |
| `led on\|off\|toggle` | LD2(PA5) 제어 |
| `uptime` | 부팅 후 경과 시간 (`HAL_GetTick`) |
| `echo <text...>` | 입력 문자열 출력 |
| `peek <hex_addr>` | 32비트 워드 읽기 (4바이트 정렬, 예: `peek 0x40010800` = `GPIOA->CRL`) |
| `reset` | `NVIC_SystemReset()` 소프트웨어 리셋 |

## 주의

`peek`는 읽기 전용이지만 존재하지 않는 주소를 읽으면 BusFault → HardFault가 난다. 진단 방법은 `08_Clock_System/08_HardFault_Diagnosis_Reg_c` 참고.

## ISR vs 콜백

`USART2_IRQHandler`(진짜 ISR) → `HAL_UART_IRQHandler()` → `HAL_UART_RxCpltCallback()`(이 파일).

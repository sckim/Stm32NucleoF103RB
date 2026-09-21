# 06_ReadPinReg

> 계층: **레지스터 직접 제어(CMSIS)** (PlatformIO `framework = cmsis`), 입력 방식: **폴링**

`04_ReadPin`의 레지스터 버전. `GPIOC->IDR`를 직접 읽어 B1(PC13) 상태에 따라 LD2(PA5)를 제어한다. (폴더에 Proteus 시뮬레이션 파일 `STM32F103R.pdsprj`도 포함되어 있다.)

## 핵심 학습 내용

- **입력 핀 설정 (`GPIOC->CRH`)**: PC13은 상위 레지스터(`CRH`)의 5번째 필드(`(13-8)*4`)이다. `CNF=10, MODE=00` = **풀업/풀다운 입력**이며, `ODR` 비트 13을 1로 쓰면 풀업이 선택된다.
- **클럭 허용**: `RCC->APB2ENR`의 `IOPAEN | IOPCEN` (GPIOA, GPIOC).
- **입력 읽기**: `GPIOC->IDR & (1 << 13)`. `IDR`은 읽기 전용이며 핀의 실제 논리 레벨을 반영한다.
- `CRL`(핀 0~7)과 `CRH`(핀 8~15)로 나뉘는 구조와, 비트 필드를 안전하게 바꾸는 패턴 `reg &= ~mask; reg |= value;`.
- 출력은 `BSRR`(`BS5`/`BR5`)로 원자적 제어.

## 동작

버튼이 안 눌린 상태(High)면 LD2 ON, 눌리면 OFF (`04_ReadPin`과 같은 논리). `SysTick_Init()`이 호출되지만 이 예제의 루프는 딜레이를 쓰지 않는다.

## 관련 예제

`04_ReadPin`(HAL 버전), `03_BlinkReg`(출력 레지스터 설정).

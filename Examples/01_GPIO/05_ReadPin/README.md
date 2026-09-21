# 05_ReadPin

> 계층: **HAL** (STM32CubeMX 생성 프로젝트), 입력 방식: **폴링**

사용자 버튼(B1, PC13)의 상태를 `while` 루프에서 계속 읽어 LD2(PA5)를 제어한다.

## 핵심 학습 내용

- **GPIO 입력 읽기**: `HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin)`
- **Active-Low 버튼**: Nucleo의 B1은 보드에서 풀업되어 있어 평소 High, 눌리면 Low이다. 코드는 `SET`(안 눌림)일 때 LED를 켜고 눌렀을 때 끄므로, 눌러 보면 LED가 꺼지는 동작을 확인할 수 있다. 논리를 반대로 만들려면 조건을 뒤집는다.
- **폴링(polling)의 특징**: 구현이 단순하지만 루프가 입력 확인에만 CPU를 쓰고, 루프 안에 오래 걸리는 작업이 있으면 입력을 놓친다. → `06_ExtInt`에서 인터럽트 방식과 비교한다.
- CubeMX에서 핀을 `GPIO_Input`(B1)과 `GPIO_Output`(LD2)으로 지정하는 방법. (`USART2`도 함께 생성되어 있으나 이 예제에서는 사용하지 않는다.)

## 동작

B1 상태를 읽어 `HAL_GPIO_WritePin(LD2, SET/RESET)` 로 LED를 갱신한다 (`USER CODE BEGIN 3`).

## 관련 예제

레지스터 버전 `07_ReadPinReg`, 인터럽트 버전 `06_ExtInt`.

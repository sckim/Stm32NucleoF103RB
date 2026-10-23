# 08_ExternalInt

> 계층: **HAL + EXTI/NVIC** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

서로 다른 두 EXTI 벡터(`EXTI9_5`, `EXTI15_10`)에 걸린 버튼 두 개를 **하나의 `HAL_GPIO_EXTI_Callback`** 에서 구분해 처리한다.

## 핵심 학습 내용

- **EXTI 라인 → NVIC 벡터 매핑**: EXTI5~9는 `EXTI9_5_IRQn`, EXTI10~15는 `EXTI15_10_IRQn`를 공유한다. 각 벡터를 따로 허용하고 우선순위를 지정해야 한다.
- **콜백 하나로 여러 핀 처리**: 핀 번호가 매개변수(`GPIO_Pin`)로 전달되므로 `if (GPIO_Pin == B1_Pin) ... if (GPIO_Pin == B2_Pin) ...` 로 분기한다.
- **에지 선택**: B1(PC13)은 하강 에지(`GPIO_MODE_IT_FALLING`), B2(PA6)는 상승 에지(`GPIO_MODE_IT_RISING`, 풀 없음)로 설정되어 있다. 두 버튼의 극성(Active-Low / Active-High)이 다르다는 점을 확인한다.
- 같은 번호의 EXTI 라인은 한 포트만 쓸 수 있다 (예: EXTI6는 PA6 / PB6 / … 중 하나). 이 제약을 `AFIO->EXTICR`가 정한다.

## 06_ExtInt와의 차이

두 예제 모두 EXTI 인터럽트로 버튼을 읽어 LD2를 토글하는 기본 동작은 같다. 이 예제는 06에 **버튼(EXTI 라인)을 하나 더 추가**해서, 라인이 여러 개일 때 생기는 문제를 다룬다.

| | `06_ExtInt` | `08_ExternalInt` (이 예제) |
|---|---|---|
| 목적 | EXTI 기본 흐름 익히기 | 여러 EXTI 라인·벡터를 구분해 처리하기 |
| 버튼 | B1(PC13) 1개 | B1(PC13) + B2(PA6) |
| EXTI 라인 | EXTI13 | EXTI13, EXTI6 |
| NVIC 벡터 | `EXTI15_10_IRQn` 1개 | `EXTI15_10_IRQn` + `EXTI9_5_IRQn` 2개 |
| ISR (`stm32f1xx_it.c`) | `EXTI15_10_IRQHandler` 1개 | `EXTI15_10_IRQHandler`(B1) + `EXTI9_5_IRQHandler`(B2) |
| 콜백 | `GPIO_Pin == B1_Pin` 하나만 확인 | 같은 `HAL_GPIO_EXTI_Callback`에서 B1/B2를 `if`로 구분 |
| 에지 | B1 하강 | B1 하강, B2 상승 |
| 배선 | 보드만 있으면 됨 | PA6에 외부 버튼 필요 |

- **ISR은 벡터마다 따로, 콜백은 하나**: 벡터가 둘이므로 ISR도 둘이지만, 두 ISR 모두 `HAL_GPIO_EXTI_IRQHandler()`를 거쳐 **같은 콜백**을 부른다. 어느 핀에서 왔는지는 콜백의 `GPIO_Pin` 인자로만 알 수 있다.
- **NVIC 설정이 늘어난다**: 06은 `EXTI15_10_IRQn`만 허용하면 되지만, 여기서는 `EXTI9_5_IRQn`도 `HAL_NVIC_SetPriority` + `HAL_NVIC_EnableIRQ`로 허용해야 한다. 빠뜨리면 B2는 아무 반응이 없다.
- **LED만으로는 구분이 안 된다**: 두 버튼 모두 같은 LD2를 토글하므로 어느 버튼이 눌렸는지는 LED로 알 수 없다. 콜백의 `if` 분기에 브레이크포인트를 걸거나, 분기마다 다른 동작(예: 다른 LED, UART 출력)을 넣어 확인해 본다.
- **극성이 다른 입력**: 06의 B1은 보드에 풀업이 있는 Active-Low 입력이라 누르는 순간이 하강 에지다. B2는 외부 풀다운 + 3.3V 연결의 Active-High 입력이라 상승 에지로 감지한다. 에지 선택은 회로 극성에 맞춰야 한다.

## 하드웨어 / 배선

- B1 = 보드 사용자 버튼(PC13)
- B2 = **외부 버튼을 PA6(Arduino D12)에 연결**. `GPIO_NOPULL`이므로 상승 에지가 깨끗하게 나오도록 외부 풀다운(예: 10kΩ)을 두고, 버튼은 3.3V로 연결한다.

## 동작

어느 버튼이든 누를 때마다 LD2(PA5)가 토글된다. (ISR 문맥의 콜백이므로 처리는 `HAL_GPIO_TogglePin`만 수행.)

## 관련 예제

단일 EXTI [`06_ExtInt`](../06_ExtInt)(위 "06_ExtInt와의 차이" 참고).

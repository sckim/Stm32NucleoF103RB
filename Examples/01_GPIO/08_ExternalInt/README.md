# 08_ExternalInt

> 계층: **HAL + EXTI/NVIC** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

서로 다른 두 EXTI 벡터(`EXTI9_5`, `EXTI15_10`)에 걸린 버튼 두 개를 **하나의 `HAL_GPIO_EXTI_Callback`** 에서 구분해 처리한다.

## 핵심 학습 내용

- **EXTI 라인 → NVIC 벡터 매핑**: EXTI5~9는 `EXTI9_5_IRQn`, EXTI10~15는 `EXTI15_10_IRQn`를 공유한다. 각 벡터를 따로 허용하고 우선순위를 지정해야 한다.
- **콜백 하나로 여러 핀 처리**: 핀 번호가 매개변수(`GPIO_Pin`)로 전달되므로 `if (GPIO_Pin == B1_Pin) ... if (GPIO_Pin == B2_Pin) ...` 로 분기한다.
- **에지 선택**: B1(PC13)은 하강 에지(`GPIO_MODE_IT_FALLING`), B2(PA6)는 상승 에지(`GPIO_MODE_IT_RISING`, 풀 없음)로 설정되어 있다. 두 버튼의 극성(Active-Low / Active-High)이 다르다는 점을 확인한다.
- 같은 번호의 EXTI 라인은 한 포트만 쓸 수 있다 (예: EXTI6는 PA6 / PB6 / … 중 하나). 이 제약을 `AFIO->EXTICR`가 정한다.

## 하드웨어 / 배선

- B1 = 보드 사용자 버튼(PC13)
- B2 = **외부 버튼을 PA6(Arduino D12)에 연결**. `GPIO_NOPULL`이므로 상승 에지가 깨끗하게 나오도록 외부 풀다운(예: 10kΩ)을 두고, 버튼은 3.3V로 연결한다.

## 동작

어느 버튼이든 누를 때마다 LD2(PA5)가 토글된다. (ISR 문맥의 콜백이므로 처리는 `HAL_GPIO_TogglePin`만 수행.)

## 관련 예제

단일 EXTI `06_ExtInt`.

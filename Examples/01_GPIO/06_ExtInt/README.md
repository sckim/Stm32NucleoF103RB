# 06_ExtInt

> 계층: **HAL + EXTI/NVIC** (STM32CubeIDE 프로젝트 구조: `Src`, `Inc`, `Startup`, `.ioc`)

버튼(B1, PC13)을 **외부 인터럽트(EXTI)** 로 처리해, 눌릴 때마다 LD2(PA5)를 토글한다.

## 핵심 학습 내용

- **인터럽트 방식 입력**: 핀을 `GPIO_MODE_IT_FALLING`(하강 에지 인터럽트)으로 설정하면 버튼이 눌리는 순간(High→Low)에 하드웨어가 CPU를 호출한다. main 루프는 비어 있어도 된다.
- **EXTI 경로**: PC13 → `EXTI13` → NVIC `EXTI15_10_IRQn`(EXTI10~15 공유 벡터). `HAL_NVIC_SetPriority` + `HAL_NVIC_EnableIRQ`로 허용.
- **ISR과 콜백의 구분**
  - `EXTI15_10_IRQHandler`(`stm32f1xx_it.c`, 진짜 ISR) → `HAL_GPIO_EXTI_IRQHandler()`가 펜딩 비트(`EXTI->PR`)를 지우고 →
  - `HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)`(이 예제의 `main.c`, 사용자 콜백)를 호출한다.
- 콜백은 ISR 문맥에서 실행되므로 `HAL_Delay()` 등 오래 걸리는 처리를 넣지 않고 짧게 끝낸다. 매개변수 `GPIO_Pin`으로 어느 핀에서 발생했는지 구분한다.
- 이 예제에는 디바운스 처리가 없다. 기계식 버튼의 접점 튐 때문에 한 번 눌러도 여러 번 토글될 수 있는지 관찰해 볼 만하다 (→ `08_Clock_System/09_NonBlocking_StateMachine_HAL_c`에 디바운스 예).

## 관련 예제

폴링 방식 `05_ReadPin`, 여러 EXTI 라인 `08_ExternalInt`, 저전력 웨이크업에 EXTI를 쓰는 `09_WatchDog_Sleep/04_Stop_Mode_EXTI_HAL_c`.

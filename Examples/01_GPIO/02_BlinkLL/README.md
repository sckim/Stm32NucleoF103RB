# 02_BlinkLL

> 계층: **LL (Low-Layer)** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

LL API로 LD2(PA5)를 1초 간격으로 토글한다.

## 핵심 학습 내용

- **HAL과 LL의 차이**: LL은 핸들 구조체·상태 관리 없이 레지스터를 거의 1:1로 감싼 인라인 함수 모음이라 코드가 작고 빠르다. 대신 이식성과 편의 기능(타임아웃, 에러 코드)은 적다.
- `LL_GPIO_TogglePin`(내부적으로 `ODR` 반전), `LL_mDelay`(SysTick 기반 블로킹 딜레이)
- **CubeMX가 생성한 초기화 코드 읽기**
  - `LL_APB2_GRP1_EnableClock(AFIO)`, `LL_APB1_GRP1_EnableClock(PWR)`: 주변장치 클럭 허용 (`RCC->APB2ENR`, `APB1ENR`)
  - `NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)`: 우선순위 4비트 전부 선점 우선순위로 사용
  - `LL_GPIO_AF_Remap_SWJ_NOJTAG()`: `AFIO->MAPR.SWJ_CFG` 로 JTAG-DP를 끄고 SW-DP만 유지 (PA15/PB3/PB4 해방). 자세한 내용은 `08_Clock_System/73_AFIO_Remap_Reg_c` 참고.

## 동작

`LL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin)` → `LL_mDelay(1000)` 반복.

## 관련 예제

`01_BlinkHAL`(HAL), `03_BlinkReg`(레지스터)와 코드량과 가독성을 비교한다.

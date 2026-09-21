# 02_BlinkHAL

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

HAL API로 LD2(PA5)를 100ms 간격으로 토글한다.

## 핵심 학습 내용

- **CubeMX 프로젝트 구조**: `Blink.ioc`에서 핀/클럭을 설정하면 `main.c`, `stm32f1xx_hal_msp.c`, `stm32f1xx_it.c` 등이 생성된다.
- **`USER CODE` 영역 규칙**: 사용자 코드는 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 안에만 둬야 재생성 시 보존된다. 이 예제의 사용자 코드는 `USER CODE BEGIN 3`(while 루프 안)의 `HAL_GPIO_TogglePin` + `HAL_Delay`뿐이다.
- **표준 초기화 순서**: `HAL_Init()`(SysTick/Flash 초기화) → `SystemClock_Config()` → `MX_GPIO_Init()`.
- **클럭 트리**: HSI(8MHz)/2 × PLL16 = 64MHz, APB1 = /2, Flash 웨이트 스테이트 2.
- `HAL_Delay()`는 SysTick 1ms 틱(`HAL_GetTick`)에 기반한 **블로킹** 대기이다.

## 동작

`HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin)` → `HAL_Delay(100)` 반복.

## 관련 예제

같은 동작의 `01_BlinkArduino`(더 추상적), `03_BlinkLL`, `04_BlinkReg`(더 낮은 수준)와 비교한다.

# 03_BlinkLL

> 계층: **LL** (STM32CubeMX 생성 프로젝트, Driver Selector = LL, PlatformIO `framework = stm32cube` + `-D USE_FULL_LL_DRIVER`)

LL(Low-Layer) API로 LD2(PA5)를 1초(1000ms) 간격으로 토글한다 (1초 켜짐 / 1초 꺼짐, 점멸 주기 2초). `01_BlinkArduino → 02_BlinkHAL → 03_BlinkLL → 04_BlinkReg` 시리즈의 세 번째이다.

## 핵심 학습 내용

- **LL의 성격**: HAL처럼 핸들 구조체·상태 머신·타임아웃이 없고, 레지스터 접근을 `static inline` 함수로 감싼 정도이다. 콜백(`HAL_..._Callback`)도 없다.
- **초기화 순서**: `HAL_Init()`이 없다. `main()`이 직접 `LL_APB2_GRP1_EnableClock(AFIO)`, `LL_APB1_GRP1_EnableClock(PWR)`, `NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)`를 호출한 뒤 `SystemClock_Config()` → `MX_GPIO_Init()` 순서로 진행한다.
- **JTAG 해제**: `LL_GPIO_AF_Remap_SWJ_NOJTAG()`(`AFIO_MAPR.SWJ_CFG = 010`)로 JTAG을 끄고 SWD만 남긴다. PA15·PB3·PB4가 일반 GPIO로 풀린다. AFIO 클럭을 먼저 켜야 이 쓰기가 반영된다.
- **클럭 트리**: HSE Bypass 8MHz(ST-LINK MCO) × PLL8 = SYSCLK 64MHz, AHB /1, APB1 /2(32MHz), APB2 /1, Flash 웨이트 스테이트 2. `LL_RCC_*` 함수가 `RCC_CR`/`RCC_CFGR`를 순서대로 설정하고, 준비 플래그(HSERDY, PLLRDY, SWS)를 `while`로 기다린다. (`02_BlinkHAL`은 HSI/2 × PLL16으로 같은 64MHz를 만든다.)
- **1ms 틱과 딜레이**: `LL_Init1msTick(64000000)`이 `SysTick_LOAD = 63999`, `SysTick_CTRL = 0x5`(CLKSOURCE=1, ENABLE=1, **TICKINT=0**)로 설정한다. `LL_mDelay(1000)`은 인터럽트 없이 `SysTick_CTRL.COUNTFLAG`를 폴링하며, 최소 대기를 보장하려고 1ms를 더해 약 1001ms를 기다린다. 그래서 `SysTick_Handler`는 비어 있다 (HAL의 `HAL_IncTick` 계층이 없다).
- **GPIO 출력**: `LL_GPIO_Init()`으로 PA5를 Push-Pull 출력, `LL_GPIO_SPEED_FREQ_LOW`(`MODE5 = 10`, 2MHz)로 설정한다. `LL_GPIO_TogglePin()`은 `ODR`을 읽어 `BSRR`에 한 번 쓴다.

## 동작

`LL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin)` → `LL_mDelay(1000)` 반복 (`USER CODE BEGIN 3` 영역).

## CubeMX 설정이 남긴 부분 (이 예제의 동작과 무관)

| 항목 | 생성 코드 | 영향 |
|---|---|---|
| B1(PC13) | 플로팅 입력, `AFIO_EXTICR4` = 포트 C, EXTI13 **상승 에지**, `EXTI15_10_IRQn` NVIC 허용(우선순위 0) | 버튼을 뗄 때 `EXTI15_10_IRQHandler`가 실행되지만 플래그(`EXTI_PR.PR13`)만 지우고 끝난다 |
| PA2 / PA3 | AF Push-Pull / 플로팅 입력 (USART2 핀) | USART2는 초기화하지 않으므로 사용되지 않는다 |
| `stm32f1xx_hal_msp.c` | 없음 | LL 프로젝트이므로 MSP 콜백이 생성되지 않는다 |

## platformio.ini

```ini
[platformio]
src_dir = Core/Src
include_dir = Core/Inc

[env:nucleo_f103rb]
platform = ststm32
board = nucleo_f103rb
framework = stm32cube
build_type = debug
monitor_port = COM5
monitor_speed = 115200
build_flags =
    -D USE_FULL_LL_DRIVER
    -D STM32F103xB
```

- `USE_FULL_LL_DRIVER`가 있어야 `LL_GPIO_Init()`, `LL_EXTI_Init()`, `LL_Init1msTick()` 같은 초기화·유틸리티 함수가 빌드된다.
- `monitor_port`는 작성자 PC 기준이므로 환경에 맞게 수정한다. 이 예제는 시리얼 출력을 사용하지 않는다.

## 확인 방법

빌드·업로드 후 LD2(초록 LED)가 1초 켜짐 / 1초 꺼짐으로 점멸하면 성공이다.

## 관련 예제

`02_BlinkHAL`(같은 동작의 HAL 버전), `04_BlinkReg`(레지스터 직접 제어). 예제설명: `03_BlinkLL 예제설명.html`.

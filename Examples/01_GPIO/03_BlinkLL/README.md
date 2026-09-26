# 03_BlinkLL

> 계층: **LL (Low-Layer)** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

LL API로 LD2(PA5)를 1초(1000ms) 간격으로 토글한다.

> CubeMX 프로젝트 만드는 법, 핀·클럭 설정, `USER CODE` 영역 규칙, 빌드·업로드 절차는 [02_BlinkHAL](../02_BlinkHAL/README.md)과 같다.
> 이 문서는 **HAL 예제와 달라지는 부분만** 다룬다.

## 동작

`LL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin)` → `LL_mDelay(1000)` 반복.

## 핵심 학습 내용

### 1. HAL과 LL의 차이

| 구분 | HAL (`02_BlinkHAL`) | LL (이 예제) |
|---|---|---|
| 구현 형태 | 핸들 구조체, 상태 관리, 타임아웃·에러 코드 반환 | 대부분 `__STATIC_INLINE` 함수. 레지스터 접근을 거의 1:1로 감쌈 |
| 초기화 진입점 | `HAL_Init()` | 없음. `main()`이 필요한 레지스터를 직접 설정 |
| 설정 헤더 | `stm32f1xx_hal_conf.h` 필요 | 필요 없음. `main.h`가 `stm32f1xx_ll_*.h`를 직접 include |
| 핀 매크로 | `GPIO_PIN_5` (= 비트 마스크 `0x0020`) | `LL_GPIO_PIN_5` (BSRR 마스크 + CRL/CRH 위치 정보가 인코딩됨) |
| 딜레이 | `HAL_Delay()` — SysTick **인터럽트**가 올리는 `uwTick` 비교 | `LL_mDelay()` — SysTick **COUNTFLAG 폴링** |
| 코드 크기·속도 | 크고 느림, 이식성 높음 | 작고 빠름, 칩 레지스터 구조를 알아야 함 |

`LL_GPIO_PIN_x` 값에는 CRL/CRH 선택 정보가 섞여 있으므로 HAL의 `GPIO_PIN_x`와 섞어 쓰면 안 된다.

### 2. `LL_GPIO_TogglePin()` — BSRR 한 번 쓰기

```c
uint32_t odr = READ_REG(GPIOx->ODR);                       /* 현재 출력 상태 읽기 */
WRITE_REG(GPIOx->BSRR, ((odr & pinmask) << 16u)            /* 켜진 핀 → BRy(상위 16비트)로 리셋 */
                       | (~odr & pinmask));                /* 꺼진 핀 → BSy(하위 16비트)로 셋 */
```

`ODR`에 read-modify-write를 하지 않고 `GPIOA_BSRR`에 한 번만 쓰므로, 같은 포트의 다른 핀을 인터럽트에서 바꾸더라도 경쟁 상태가 생기지 않는다.

### 3. SysTick 사용 방식 — 인터럽트 없는 1ms 틱

`SystemClock_Config()` 끝에서 호출되는 `LL_Init1msTick(64000000)`은 다음만 설정한다.

- `SysTick->LOAD = 64000000/1000 - 1` → 1ms마다 0에 도달
- `SysTick->CTRL = CLKSOURCE | ENABLE` → **TICKINT 비트는 켜지 않음**

따라서 `SysTick_Handler()`(ISR, `stm32f1xx_it.c`)는 생성은 되지만 호출되지 않으며 내용도 비어 있다.
`LL_mDelay(n)`은 `SysTick->CTRL`의 **COUNTFLAG**(카운터가 0을 지날 때 1, 읽으면 자동 클리어)를 n+1번 확인할 때까지 바쁜 대기(busy-wait)한다.

- HAL처럼 틱 카운터(`HAL_GetTick()`)가 없으므로, 딜레이 중 경과 시간을 다른 곳에서 읽을 수 없다.
- 틱 카운터가 필요하면 `LL_SYSTICK_EnableIT()`로 TICKINT를 켜고 `SysTick_Handler()`의 `USER CODE BEGIN SysTick_IRQn 0` 영역에서 직접 카운트해야 한다.

### 4. CubeMX가 생성한 LL 초기화 코드 읽기

`HAL_Init()`/`HAL_MspInit()`이 하던 일이 `main()` 앞부분에 직접 펼쳐져 있다.

| 코드 | 다루는 레지스터·비트 | 의미 |
|---|---|---|
| `LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO)` | `RCC_APB2ENR.AFIOEN` | AFIO 클럭 허용 (리맵·EXTI 소스 선택에 필요) |
| `LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR)` | `RCC_APB1ENR.PWREN` | PWR 클럭 허용 |
| `NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)` | `SCB_AIRCR.PRIGROUP` | 우선순위 4비트를 모두 선점(preemption) 우선순위로 사용 |
| `LL_GPIO_AF_Remap_SWJ_NOJTAG()` | `AFIO_MAPR.SWJ_CFG = 010` | JTAG-DP 끄고 SW-DP만 유지 → PA15/PB3/PB4를 GPIO로 사용 가능. 자세한 내용은 `08_Clock_System/07_AFIO_Remap_Reg_c` 참고 |

`SystemClock_Config()`는 HAL의 구조체 방식 대신 **설정 → Ready 플래그 대기**를 순서대로 나열한다. 클럭 트리 값 자체는 HAL 예제와 같다.

```
LL_FLASH_SetLatency(2)          FLASH_ACR.LATENCY      (클럭을 올리기 전에 먼저!)
LL_RCC_HSE_EnableBypass/Enable  RCC_CR.HSEBYP, HSEON → HSERDY 대기
LL_RCC_PLL_ConfigDomain_SYS     RCC_CFGR.PLLSRC, PLLMUL(×8)
LL_RCC_PLL_Enable               RCC_CR.PLLON → PLLRDY 대기
LL_RCC_SetAHB/APB1/APB2Prescaler RCC_CFGR.HPRE, PPRE1(/2), PPRE2(/1)
LL_RCC_SetSysClkSource(PLL)     RCC_CFGR.SW → SWS 확인
LL_Init1msTick / LL_SetSystemCoreClock(64MHz)
```

`MX_GPIO_Init()`은 `LL_GPIO_InitTypeDef`/`LL_EXTI_InitTypeDef` 구조체를 채워 `LL_GPIO_Init()`/`LL_EXTI_Init()`에 넘긴다. 이 두 함수는 인라인이 아닌 **LL 드라이버 `.c` 파일**에 있다 (아래 `USE_FULL_LL_DRIVER` 참고).

> 참고: 보드 기본 핀 설정 때문에 PC13(B1)이 `GPIO_EXTI13`으로 잡혀 있어 `AFIO_EXTICR4`, `EXTI_IMR`/`EXTI_RTSR`의 13번 비트도 설정된다. 그러나 NVIC에서 `EXTI15_10_IRQn`을 켜지 않았으므로 ISR은 생성되지 않으며, 이 예제의 동작과는 무관하다. 버튼 인터럽트는 `06_ExtInt`에서 다룬다.

## HAL 예제와 달라지는 진행 순서

### CubeMX: 드라이버를 LL로 바꾸기

핀·클럭 설정([02_BlinkHAL 2~3단계](../02_BlinkHAL/README.md#2-pinout--configuration))을 마친 뒤, GENERATE CODE 전에 다음을 바꾼다.

1. **Project Manager > Advanced Settings > Driver Selector**에서 **RCC**, **GPIO**를 `HAL` → `LL`로 변경.
2. 그대로 GENERATE CODE.

생성 결과의 차이:

- `Core/Inc/stm32f1xx_hal_conf.h`, `Core/Src/stm32f1xx_hal_msp.c`가 **생성되지 않는다**.
- `Drivers/STM32F1xx_HAL_Driver`에는 `stm32f1xx_ll_*.h/.c`만 복사된다.
- `main.h`가 `stm32f1xx_ll_*.h`를 include하고, `LD2_Pin`이 `LL_GPIO_PIN_5`로 정의된다.
- `.ioc`의 `ProjectManager.functionlistsort` 항목에 `...-LL-...`로 기록된다.

### PlatformIO: `USE_FULL_LL_DRIVER` 정의

`platformio.ini`는 HAL 예제와 같되 `build_flags`가 추가된다.

```ini
build_flags =
	-DUSE_FULL_LL_DRIVER
```

LL 드라이버 `.c` 파일(`stm32f1xx_ll_gpio.c`, `_rcc.c`, `_exti.c` 등)은 전체가 `#if defined(USE_FULL_LL_DRIVER)`로 감싸져 있다. 이 매크로가 없으면 `LL_GPIO_Init()`, `LL_EXTI_Init()` 등이 컴파일되지 않아 **링크 에러(undefined reference)** 가 난다. (STM32CubeIDE는 이 매크로를 프로젝트 설정에 자동으로 넣지만, PlatformIO에서는 직접 넣어야 한다.)

### 사용자 코드

`USER CODE BEGIN 3` 영역에 다음 두 줄만 작성한다.

```c
    /* USER CODE BEGIN 3 */
    LL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); /* GPIOA_BSRR에 BS5 또는 BR5를 한 번 써서 PA5 반전 */
    LL_mDelay(1000);                           /* SysTick COUNTFLAG를 1000+1회 폴링하는 블로킹 대기 */
  }
  /* USER CODE END 3 */
```

빌드·업로드 방법과 결과(LD2가 1초 간격 점멸)는 HAL 예제와 같다.

## 관련 예제

- [02_BlinkHAL](../02_BlinkHAL/README.md)(HAL): 같은 동작. 초기화 순서와 `HAL_Delay` 구조를 비교한다.
- [04_BlinkReg](../04_BlinkReg/README.md)(레지스터): LL 함수 안의 레지스터 접근을 직접 작성한다. LL 코드와 한 줄씩 대응시켜 본다.

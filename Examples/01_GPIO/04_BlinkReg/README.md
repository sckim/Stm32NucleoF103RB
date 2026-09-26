# 04_BlinkReg

> 계층: **레지스터 직접 제어(CMSIS)** (PlatformIO `framework = cmsis`, HAL/LL 없음, CubeMX 사용 안 함)

`RCC`, `GPIOA->CRL`, `GPIOA->BSRR` 레지스터를 직접 다뤄 LD2(PA5)를 0.5초 간격으로 점멸한다. 딜레이도 SysTick 인터럽트로 직접 만든다.

> [02_BlinkHAL](../02_BlinkHAL/README.md) → [03_BlinkLL](../03_BlinkLL/README.md)에 이어지는 예제다.
> HAL/LL 예제는 **CubeMX가 만든 뼈대 위에 두 줄만 추가**했지만, 이 예제는 **CubeMX 없이 `main.c` 하나를 처음부터 작성**한다.
> 이 문서는 앞의 두 예제와 **달라지는 부분**을 중심으로 설명한다.

## 동작

`GPIOA->BSRR = GPIO_BSRR_BS5` → `delay_ms(500)` → `GPIOA->BSRR = GPIO_BSRR_BR5` → `delay_ms(500)` 반복.
(HAL/LL은 1초마다 토글, 이 예제는 0.5초 켜짐 + 0.5초 꺼짐 → 점멸 주기는 같은 1초.)

## 핵심 학습 내용

- **GPIO 설정 3단계를 레지스터로 확인**
  1. `RCC->APB2ENR |= RCC_APB2ENR_IOPAEN` : GPIOA 클럭 허용
  2. `GPIOA->CRL` 의 PA5 필드(비트 23:20)를 `CNF=00`(범용 푸시풀) + `MODE=01`(출력)으로 설정. 핀당 4비트이므로 `5 * 4` 비트 시프트.
  3. `GPIOA->BSRR = GPIO_BSRR_BS5 / BR5` : 원자적 set/reset (`ODR`을 읽고-수정-쓰기 하지 않음)
- **SysTick 기반 `delay_ms`**: `SysTick_Config(SystemCoreClock / 1000)`으로 1ms 예외를 만들고, `SysTick_Handler`가 `msTicks`(`volatile`)를 증가시킨다. `(msTicks - start) < ms` 뺄셈 비교는 32비트 오버플로에도 안전하다.
- HAL이 내부에서 하는 일을 직접 해 보며 `02_BlinkHAL`, `03_BlinkLL`의 API가 무엇을 감싸는지 이해한다.

### 1. HAL · LL · 레지스터 비교

| 구분 | HAL (`02_BlinkHAL`) | LL (`03_BlinkLL`) | 레지스터 (이 예제) |
|---|---|---|---|
| PlatformIO `framework` | `stm32cube` | `stm32cube` + `-DUSE_FULL_LL_DRIVER` | **`cmsis`** |
| 프로젝트 생성 | CubeMX `.ioc` → GENERATE CODE | CubeMX (Driver Selector = LL) | **PlatformIO New Project** (`.ioc` 없음) |
| 소스 위치 | `Core/Src`, `Core/Inc` | `Core/Src`, `Core/Inc` | **`src/main.c`** 하나 |
| 사용하는 헤더 | `stm32f1xx_hal.h` | `stm32f1xx_ll_*.h` | **`stm32f1xx.h`** (레지스터 구조체·비트 매크로만 정의) |
| `USER CODE` 영역 | 있음 (재생성 시 보존) | 있음 | **없음** — 재생성할 도구가 없으므로 파일 전체가 사용자 코드 |
| 시스템 클럭 | `SystemClock_Config()`: HSE Bypass × PLL8 = **64MHz** | 동일 (LL 함수 나열) | **설정 안 함 → 리셋 기본값 HSI 8MHz** |
| GPIO 클럭 허용 | `__HAL_RCC_GPIOA_CLK_ENABLE()` | `LL_APB2_GRP1_EnableClock(...GPIOA)` | `RCC->APB2ENR \|= RCC_APB2ENR_IOPAEN` |
| PA5 출력 설정 | `HAL_GPIO_Init()` (구조체) | `LL_GPIO_Init()` (구조체) | `GPIOA->CRL` 비트 23:20 직접 쓰기 |
| 출력 속도 (`MODE5`) | `GPIO_SPEED_FREQ_LOW` = `10`(2MHz) | `LL_GPIO_SPEED_FREQ_LOW` = `10`(2MHz) | **`01`(10MHz)** — LED에는 차이 없음 |
| LED 제어 | `HAL_GPIO_TogglePin()` | `LL_GPIO_TogglePin()` (ODR 읽고 BSRR 1회 쓰기) | `BSRR`에 `BS5`/`BR5` 직접 쓰기 (ODR 읽지 않음) |
| 딜레이 | `HAL_Delay()` — SysTick **인터럽트** + `uwTick` | `LL_mDelay()` — SysTick **COUNTFLAG 폴링** | `delay_ms()` — SysTick **인터럽트** + `msTicks` (HAL 방식을 직접 구현) |
| `SysTick_Handler` 위치 | `stm32f1xx_it.c` (CubeMX 생성) → `HAL_IncTick()` | `stm32f1xx_it.c` (비어 있음, 호출 안 됨) | **`main.c`에 직접 작성** (startup의 weak 심볼 재정의) |

### 2. 클럭을 설정하지 않았는데 어떻게 동작하나?

`framework = cmsis`의 startup 코드(`startup_stm32f103xb.s`)는 `main()` 전에 `SystemInit()`을 호출하지만,
PlatformIO가 제공하는 `system_stm32f1xx.c`의 `SystemInit()`은 PLL을 켜지 않는다. 따라서 칩은 **리셋 기본값인 HSI 8MHz**로 동작하고,
`SystemCoreClock` 변수도 초기값 `8000000`이다.

- `SysTick_Config(SystemCoreClock / 1000)` → `SysTick->LOAD = 8000 - 1` → 8MHz에서 정확히 1ms. 그래서 딜레이는 맞다.
- HAL/LL 예제와 **CPU 속도는 8배 차이**(8MHz vs 64MHz)가 나지만, LED 점멸 주기는 SysTick이 1ms를 보장하므로 같다.
- 64MHz로 올리려면 LL 예제의 `SystemClock_Config()` 순서(Flash Latency → HSE → PLL → 프리스케일러 → SW)를 레지스터로 직접 작성해야 한다. `08_Clock_System/04_Clock_Config_Reg_c`에서 다룬다.
  클럭을 바꾼 뒤에는 `SystemCoreClockUpdate()`를 호출해야 `SysTick_Config()`에 넘기는 값이 맞는다.

### 3. SysTick 사용 방식 — HAL을 흉내 낸 인터럽트 틱

| 레지스터·비트 | `SysTick_Config()`가 쓰는 값 |
|---|---|
| `SysTick->LOAD` | `SystemCoreClock/1000 - 1` (= 7999) |
| `SCB->SHP[11]` (SysTick 우선순위) | 가장 낮은 우선순위 |
| `SysTick->VAL` | 0 (카운터 초기화) |
| `SysTick->CTRL` | `CLKSOURCE`(HCLK) \| **`TICKINT`**(예외 허용) \| `ENABLE` |

LL 예제와 달리 **`TICKINT`를 켜므로** 1ms마다 `SysTick_Handler`(ISR)가 실제로 호출된다.
ISR에서 공유하는 `msTicks`는 반드시 `volatile`로 선언해야 컴파일러가 `delay_ms()`의 while 루프에서 값을 레지스터에 캐싱하지 않는다.

## ISR

`SysTick_Handler` (벡터 테이블의 weak 심볼 재정의). 콜백 개념은 없다.

- HAL: ISR(`SysTick_Handler`) → `HAL_IncTick()` → (필요하면) 사용자가 `HAL_SYSTICK_Callback` 등 콜백 구현
- 레지스터: ISR 안에 할 일(`msTicks++`)을 직접 쓴다. 함수 이름이 startup 파일의 벡터 이름과 **정확히 같아야** 한다 (오타가 나면 weak 기본 핸들러 `Default_Handler`의 무한 루프로 빠진다).

## 진행 순서 (HAL/LL 예제와 비교)

```
HAL/LL : ① CubeMX 새 프로젝트 → ② 핀 → ③ 클럭 → ④ Project Manager → ⑤ GENERATE → ⑥ platformio.ini 추가 → ⑦ USER CODE 작성 → ⑧ 빌드·업로드
Reg    : ① PlatformIO 새 프로젝트(framework = CMSIS)            → ② main.c 전체 작성(클럭·GPIO·SysTick·ISR 직접) → ③ 빌드·업로드 → ④ 디버거로 레지스터 확인
```

CubeMX가 해 주던 ②~⑤단계(핀·클럭 설정, 초기화 코드 생성)를 **사람이 레퍼런스 매뉴얼(RM0008)을 보고 직접 코드로 작성**하는 것이 핵심 차이다.

### 0. 준비물

- VS Code + PlatformIO 확장 (CubeMX는 필요 없음)
- NUCLEO-F103RB 보드, USB 케이블(ST-LINK 드라이버 설치)
- 참고 문서: RM0008 (STM32F10x Reference Manual) — 7장 RCC(`RCC_APB2ENR`), 9장 GPIO(`GPIOx_CRL`, `GPIOx_BSRR`), PM0056 (Cortex-M3 Programming Manual) — SysTick

### 1. PlatformIO 새 프로젝트 만들기

1. PlatformIO Home → **New Project**
2. Name: `04_BlinkReg`, Board: **ST Nucleo F103RB**, Framework: **CMSIS** 선택 → Finish.
3. `src/`, `include/`, `lib/`, `test/`, `platformio.ini`가 생성된다. (`include/`, `lib/`, `test/`는 PlatformIO 기본 템플릿으로 이 예제에서는 비어 있다.)

생성된 `platformio.ini`는 HAL/LL 예제와 달리 `[platformio] src_dir/include_dir` 지정이 **필요 없다** (기본 `src/`, `include/` 사용).

```ini
[env:nucleo_f103rb]
platform = ststm32
board = nucleo_f103rb
framework = cmsis        ; HAL/LL 드라이버 없이 CMSIS 헤더 + startup + system 파일만 링크
build_type = debug       ; 디버거에서 변수·레지스터를 보기 위해
```

- `framework = cmsis`이면 `stm32f1xx.h`(→ `stm32f103xb.h`), `core_cm3.h`, startup 어셈블리, `system_stm32f1xx.c`, 링커 스크립트만 제공된다.
- `stm32f1xx_hal.h`나 `stm32f1xx_ll_gpio.h`를 include하면 **파일을 찾지 못해 컴파일 에러**가 난다. 이 차이로 "지금 어느 계층을 쓰는지"를 확인할 수 있다.
- `monitor_port`(COM5)는 작성자 PC 기준이며, 이 예제는 시리얼 출력을 사용하지 않는다.

### 2. `src/main.c` 작성

CubeMX가 없으므로 HAL/LL 예제에서 **생성 코드가 하던 일을 순서대로 직접** 쓴다. 대응 관계는 다음과 같다.

| HAL/LL 예제에서 누가 했나 | 이 예제의 코드 | 다루는 레지스터·비트 |
|---|---|---|
| startup → `SystemInit()` | (자동, 작성 안 함) | `RCC_CR`, `RCC_CFGR` 리셋값 복원 → HSI 8MHz |
| `HAL_Init()` / `LL_Init1msTick()` | `SysTick_Init()` → `SysTick_Config(SystemCoreClock / 1000)` | `SysTick->LOAD/VAL/CTRL` |
| `stm32f1xx_it.c`의 `SysTick_Handler` | `main.c`의 `SysTick_Handler()` | (ISR, 벡터 테이블 SysTick 항목) |
| `SystemClock_Config()` | **없음** (HSI 8MHz 그대로 사용) | — |
| `MX_GPIO_Init()`의 클럭 허용 | `RCC->APB2ENR \|= RCC_APB2ENR_IOPAEN` | `RCC_APB2ENR.IOPAEN`(비트 2) |
| `MX_GPIO_Init()`의 핀 설정 | `GPIOA->CRL &= ~(0xF << 20); GPIOA->CRL \|= (0x1 << 20);` | `GPIOA_CRL.CNF5[1:0]=00`, `MODE5[1:0]=01` |
| `HAL_GPIO_TogglePin` / `LL_GPIO_TogglePin` | `GPIOA->BSRR = GPIO_BSRR_BS5;` / `GPIO_BSRR_BR5` | `GPIOA_BSRR.BS5`(비트 5), `BR5`(비트 21) |
| `HAL_Delay` / `LL_mDelay` | `delay_ms()` | `msTicks` (SysTick ISR가 증가) |

작성 시 주의할 점:

- **클럭 허용이 GPIO 설정보다 먼저**여야 한다. `IOPAEN`을 켜기 전에 `GPIOA->CRL`에 쓰면 무시된다 (HAL에서는 `MX_GPIO_Init()`이 순서를 대신 지켜 줬다).
- `CRL`에 바로 `=`로 쓰면 PA0~PA7 전체 설정이 바뀐다. 반드시 **해당 4비트만 지우고(`&= ~`) 원하는 값만 넣는(`|=`)** read-modify-write를 한다.
- PA8~PA15는 `CRL`이 아니라 `CRH`를 쓴다 (예: PA10 → `CRH`의 `(10-8)*4` 위치). LL의 `LL_GPIO_PIN_x`에 CRL/CRH 정보가 인코딩되어 있던 이유가 이것이다.
- `BSRR`은 쓰기 전용이며 0을 쓴 비트는 영향이 없으므로 `|=`가 아니라 `=`로 쓴다.

### 3. 빌드 · 업로드 · 확인

1. VS Code에서 이 폴더를 연다.
2. PlatformIO 툴바 **Build**(✓) → **Upload**(→), 또는 터미널에서 `pio run -t upload`.
3. LD2(초록 LED)가 0.5초 켜짐 / 0.5초 꺼짐으로 점멸하면 성공.
4. 빌드 로그 끝의 Flash/RAM 사용량을 HAL/LL 예제와 비교해 본다. (레지스터 < LL < HAL 순으로 작다.)

### 4. 디버거로 레지스터 확인 (권장)

HAL/LL 예제에서는 함수 안에 숨어 있던 레지스터 변화를 여기서는 한 줄씩 직접 볼 수 있다.

1. PlatformIO **Debug**(F5)로 시작 → `main()`에서 정지.
2. 왼쪽 **PERIPHERALS** 패널에서 `RCC > APB2ENR`, `GPIOA > CRL`, `GPIOA > ODR`을 펼쳐 둔다.
3. F10(Step Over)으로 한 줄씩 진행하며 확인한다.
   - `RCC->APB2ENR |= ...` 후 → `IOPAEN` = 1
   - `GPIOA->CRL` 두 줄 후 → 비트 23:20 = `0001` (리셋값 `0100` = 플로팅 입력에서 변경)
   - `GPIOA->BSRR = GPIO_BSRR_BS5` 후 → `ODR5` = 1, LED 켜짐 (BSRR 자체는 읽으면 0)
4. **VARIABLES / WATCH**에 `msTicks`, `SystemCoreClock`을 추가해 `SystemCoreClock = 8000000`, `msTicks`가 1ms마다 증가하는 것을 확인한다.

## 실습 과제

앞의 HAL/LL 예제와 비교하며 코드를 바꿔 본다.

1. **HAL과 같은 코드 모양으로 바꾸기**: `BSRR` 두 번 쓰기 대신 LL의 `LL_GPIO_TogglePin()` 방식(ODR을 읽어 BSRR에 한 번 쓰기)으로 토글하고, `delay_ms(1000)`으로 HAL/LL 예제와 같은 동작을 만든다.
2. **`ODR ^= (1 << 5)`와 비교**: 동작은 같지만 read-modify-write이므로, 같은 포트 핀을 인터럽트에서 바꾸면 왜 위험한지 설명해 본다.
3. **LL 방식 딜레이로 바꾸기**: `SysTick->CTRL`에서 `TICKINT`를 끄고 `COUNTFLAG`(비트 16)를 폴링하는 `delay_ms()`를 작성해 `LL_mDelay()`와 비교한다. 이때 `SysTick_Handler`가 더 이상 호출되지 않는지 브레이크포인트로 확인한다.
4. **`volatile` 제거 실험**: `msTicks`의 `volatile`을 지우고 `build_type = release`(최적화)로 빌드하면 `delay_ms()`가 끝나지 않을 수 있음을 확인한다.
5. **출력 속도 맞추기**: `MODE5`를 `10`(2MHz)으로 바꿔 HAL/LL 예제와 같은 설정으로 만든다. (LED 점멸에는 차이가 없고, 스위칭 엣지 속도·EMI에만 영향이 있다.)

## 관련 예제

- [02_BlinkHAL](../02_BlinkHAL/README.md)(HAL), [03_BlinkLL](../03_BlinkLL/README.md)(LL): 같은 동작. 위 대응표로 한 줄씩 비교한다.
- [05_ReadPin](../05_ReadPin/README.md) → [07_ReadPinReg](../07_ReadPinReg/README.md): 입력도 같은 방식으로 HAL → 레지스터를 비교한다.
- [08_Clock_System/04_Clock_Config_Reg_c](../../08_Clock_System/04_Clock_Config_Reg_c/README.md): 이 예제에서 생략한 64MHz 클럭 설정을 레지스터로 작성.
- [08_Clock_System/05_SysTick_Reg_c](../../08_Clock_System/05_SysTick_Reg_c/README.md): SysTick을 더 자세히.
- [09_BitBanding_Reg_c](../09_BitBanding_Reg_c/README.md): `BSRR` 대신 비트 밴딩으로 한 비트를 원자적으로 쓰는 방법.

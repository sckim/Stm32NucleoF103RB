# 02_BlinkHAL

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

HAL API로 LD2(PA5)를 1초(1000ms) 간격으로 토글한다.

## 핵심 학습 내용

- **CubeMX 프로젝트 구조**: `Blink.ioc`에서 핀/클럭을 설정하면 `main.c`, `stm32f1xx_hal_msp.c`, `stm32f1xx_it.c` 등이 생성된다.
- **`USER CODE` 영역 규칙**: 사용자 코드는 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 안에만 둬야 재생성 시 보존된다. 이 예제의 사용자 코드는 `USER CODE BEGIN 3`(while 루프 안)의 `HAL_GPIO_TogglePin` + `HAL_Delay`뿐이다.
- **표준 초기화 순서**: `HAL_Init()`(SysTick/Flash 초기화) → `SystemClock_Config()` → `MX_GPIO_Init()`.
- **클럭 트리**: HSE Bypass 8MHz(ST-LINK의 MCO 출력) × PLL8 = SYSCLK 64MHz, AHB /1, APB1 /2(32MHz, 최대 36MHz 제한), APB2 /1, Flash 웨이트 스테이트 2.
- **GPIO 초기화**: `MX_GPIO_Init()`이 `__HAL_RCC_GPIOA_CLK_ENABLE()`(RCC_APB2ENR.IOPAEN)로 포트 클럭을 켠 뒤 `HAL_GPIO_Init()`으로 PA5를 Push-Pull 출력(GPIOA_CRL의 MODE5/CNF5)으로 설정한다.
- `HAL_Delay()`는 SysTick 1ms 틱(`HAL_GetTick`)에 기반한 **블로킹** 대기이다.

## 동작

`HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin)` → `HAL_Delay(1000)` 반복.

## 진행 순서

HAL 예제는 **CubeMX로 초안(뼈대 코드)을 먼저 만들고**, 그 위에 사용자 코드를 추가한 뒤 PlatformIO로 빌드·업로드하는 순서로 진행한다.

```
① CubeMX 새 프로젝트(보드 선택) → ② 핀 설정 → ③ 클럭 설정 → ④ Project Manager 설정
→ ⑤ GENERATE CODE → ⑥ platformio.ini 추가 → ⑦ USER CODE 영역에 코드 작성 → ⑧ 빌드·업로드·확인
```

### 0. 준비물

- STM32CubeMX (이 예제는 6.17.0, STM32Cube FW_F1 V1.8.7로 생성)
- VS Code + PlatformIO 확장
- NUCLEO-F103RB 보드, USB 케이블(ST-LINK 드라이버 설치)

### 1. CubeMX에서 새 프로젝트 만들기 (초안 생성)

1. CubeMX 실행 → **File > New Project** → **Board Selector** 탭에서 `NUCLEO-F103RB` 검색 후 선택 → **Start Project**.
2. "Initialize all peripherals with their default Mode?" 질문에는 **No**를 선택한다.
   - Yes를 고르면 USART2(PA2/PA3, ST-LINK 가상 COM) 등이 함께 활성화된다. 이 예제는 LED만 사용하므로 필요 없다.
   - 보드를 선택했기 때문에 No를 골라도 PA5=`LD2`, PC13=`B1 [Blue PushButton]`, SWD 핀, HSE/LSE 핀 라벨은 자동으로 잡힌다.

> 보드 대신 MCU(`STM32F103RBTx`)로 시작해도 되지만, 그 경우 아래 핀·클럭 설정을 모두 직접 해야 한다.

### 2. Pinout & Configuration

| 항목 | 설정 | 비고 |
|---|---|---|
| **PA5** | `GPIO_Output`, User Label `LD2` | 라벨 덕분에 `main.h`에 `LD2_Pin`, `LD2_GPIO_Port` 매크로가 생성된다 |
| System Core > **SYS** > Debug | `Serial Wire` | PA13(SWDIO)/PA14(SWCLK). 꺼 두면 다음 업로드·디버깅이 어려워지므로 반드시 켠다 |
| System Core > **RCC** > HSE | `BYPASS Clock Source` | Nucleo는 크리스털 대신 ST-LINK의 MCO(8MHz)를 PD0(OSC_IN)에 공급한다 |
| System Core > **GPIO** > PA5 | Output Push Pull, No pull, Low speed, 초기 출력 Low | CubeMX 기본값 그대로 |

### 3. Clock Configuration

Clock Configuration 탭에서 다음과 같이 맞춘다.

- PLL Source Mux: **HSE** (8MHz)
- PLLMul: **×8** → PLLCLK 64MHz
- System Clock Mux: **PLLCLK** → SYSCLK / HCLK 64MHz
- APB1 Prescaler: **/2** → PCLK1 32MHz (APB1은 최대 36MHz)
- APB2 Prescaler: **/1** → PCLK2 64MHz

Flash Latency(2 WS)는 CubeMX가 HCLK에 맞춰 자동 결정하며, 생성된 `SystemClock_Config()`의 `HAL_RCC_ClockConfig(..., FLASH_LATENCY_2)`에 반영된다.

### 4. Project Manager 설정

| 탭 | 항목 | 설정 |
|---|---|---|
| Project | Project Name | `Blink` |
| Project | Project Location | 이 예제 폴더의 상위 폴더 (예제 폴더에 바로 생성되도록 지정) |
| Project | Toolchain / IDE | `STM32CubeIDE` → `Core/Src`, `Core/Inc` 구조로 생성된다 |
| Code Generator | STM32Cube MCU packages | Copy only the necessary library files |
| Code Generator | Keep User Code when re-generating | ✅ 체크 (USER CODE 영역 보존) |
| Code Generator | Delete previously generated files when not re-generated | ✅ 체크 |

### 5. GENERATE CODE

우측 상단 **GENERATE CODE** 클릭. 다음 파일들이 생성된다.

- `Core/Src/main.c` — `main()`, `SystemClock_Config()`, `MX_GPIO_Init()`
- `Core/Src/stm32f1xx_hal_msp.c` — `HAL_MspInit()` (AFIO 클럭, SWD 리맵 등 저수준 초기화)
- `Core/Src/stm32f1xx_it.c` — `SysTick_Handler()` 등 **ISR**. `SysTick_Handler()`가 `HAL_IncTick()`을 호출해 `HAL_Delay()`의 1ms 틱을 만든다
- `Core/Inc/main.h`, `Core/Inc/stm32f1xx_hal_conf.h`
- `Drivers/`, `STM32F103RBTX_FLASH.ld`, `.cproject` 등 CubeIDE용 파일

### 6. PlatformIO 연결 (`platformio.ini`)

CubeMX가 만든 폴더에 `platformio.ini`를 추가해 PlatformIO가 `Core/` 구조를 인식하도록 한다.

```ini
[platformio]
src_dir = Core/Src       ; CubeMX가 생성한 소스 위치
include_dir = Core/Inc   ; CubeMX가 생성한 헤더 위치

[env:nucleo_f103rb]
platform = ststm32
board = nucleo_f103rb
framework = stm32cube
build_type = debug
```

- `framework = stm32cube`이면 HAL 드라이버는 PlatformIO의 `framework-stm32cubef1` 패키지에서 가져와 빌드하고, HAL 설정은 `Core/Inc/stm32f1xx_hal_conf.h`를 사용한다.
- `monitor_port`(COM5)는 작성자 PC 기준이므로 환경에 맞게 수정한다. 이 예제는 시리얼 출력을 사용하지 않는다.

### 7. 사용자 코드 작성

`main.c`의 while 루프 안 **`USER CODE BEGIN 3` ~ `USER CODE END 3`** 사이에만 작성한다.

```c
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); /* GPIOA_ODR.ODR5 반전 (내부적으로 BSRR 사용) */
    HAL_Delay(1000);                            /* SysTick 1ms 틱 기반 블로킹 대기 */
  }
  /* USER CODE END 3 */
```

영역 바깥에 쓴 코드는 CubeMX에서 다시 GENERATE CODE를 누를 때 지워진다.

### 8. 빌드 · 업로드 · 확인

1. VS Code에서 이 폴더를 연다 (PlatformIO가 `platformio.ini`를 인식).
2. PlatformIO 툴바의 **Build**(✓) → **Upload**(→) 또는 터미널에서 `pio run -t upload`. 업로드는 보드 내장 ST-LINK로 진행된다.
3. LD2(초록 LED)가 1초 간격으로 켜졌다 꺼지면 성공.

### 설정을 바꾸고 싶을 때 (재생성 흐름)

1. `Blink.ioc`를 CubeMX로 연다.
2. 핀/클럭 등을 수정한 뒤 **GENERATE CODE**.
3. USER CODE 영역 안의 코드는 그대로 보존되고, `MX_GPIO_Init()`/`SystemClock_Config()` 등만 새 설정으로 갱신된다.
4. PlatformIO로 다시 빌드·업로드한다. (`platformio.ini`는 CubeMX가 건드리지 않는다.)

## 관련 예제

같은 동작의 [01_BlinkArduino](../01_BlinkArduino/README.md)(더 추상적), [03_BlinkLL](../03_BlinkLL/README.md), [04_BlinkReg](../04_BlinkReg/README.md)(더 낮은 수준)와 비교한다.

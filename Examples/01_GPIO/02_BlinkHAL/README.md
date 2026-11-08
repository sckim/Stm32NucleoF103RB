# 02_BlinkHAL

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

HAL API로 LD2(PA5)를 1초(1000ms) 간격으로 토글한다.

## 핵심 학습 내용

- **CubeMX 프로젝트 구조**: `Blink.ioc`에서 핀/클럭을 설정하면 `main.c`, `stm32f1xx_hal_msp.c`, `stm32f1xx_it.c` 등이 생성된다.
- **`USER CODE` 영역 규칙**: 사용자 코드는 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 안에만 둬야 재생성 시 보존된다. 이 예제의 사용자 코드는 `USER CODE BEGIN 3`(while 루프 안)의 `HAL_GPIO_TogglePin` + `HAL_Delay`뿐이다.
- **표준 초기화 순서**: `HAL_Init()`(SysTick/Flash 초기화) → `SystemClock_Config()` → `MX_GPIO_Init()`.
- **클럭 트리**: HSI 8MHz /2 = 4MHz × PLL16 = SYSCLK 64MHz, AHB /1, APB1 /2(32MHz, 최대 36MHz 제한), APB2 /1, Flash 웨이트 스테이트 2. (`SystemClock_Config()`: `RCC_PLLSOURCE_HSI_DIV2`, `RCC_PLL_MUL16`)
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
| System Core > **RCC** > HSE | `BYPASS Clock Source` | 보드 선택 시 기본으로 잡히는 설정이다 (Nucleo는 ST-LINK의 MCO 8MHz를 PD0(OSC_IN)에 공급할 수 있다). 이 예제의 클럭 트리는 HSE를 쓰지 않고 HSI를 PLL 입력으로 쓴다 |
| System Core > **GPIO** > PA5 | Output Push Pull, No pull, Low speed, 초기 출력 Low | CubeMX 기본값 그대로 |

### 3. Clock Configuration

Clock Configuration 탭에서 다음과 같이 맞춘다.

- PLL Source Mux: **HSI/2** (8MHz / 2 = 4MHz)
- PLLMul: **×16** → PLLCLK 64MHz
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
debug_tool = stlink
upload_protocol = stlink
build_flags =
    -Wall
    -ICore/Inc
    -DUSE_HAL_DRIVER

monitor_port = COM9
monitor_speed = 115200
```

- `framework = stm32cube`이면 HAL 드라이버는 PlatformIO의 `framework-stm32cubef1` 패키지에서 가져와 빌드하고, HAL 설정은 `Core/Inc/stm32f1xx_hal_conf.h`를 사용한다.
- `monitor_port`(COM9)는 작성자 PC 기준이므로 환경에 맞게 수정한다. 이 예제는 시리얼 출력을 사용하지 않는다.

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

## Wokwi로 시뮬레이션하기 (보드 없이)

보드가 없을 때는 [Wokwi](https://wokwi.com) 시뮬레이터로 같은 펌웨어를 돌려 LED 점멸을 확인할 수 있다. 이 폴더의 `wokwi.toml`과 `diagram.json`이 그 설정이다.

| 파일 | 내용 |
|---|---|
| `wokwi.toml` | 시뮬레이터에 넣을 펌웨어 경로(`.pio/build/nucleo_f103rb/firmware.elf`) |
| `diagram.json` | 가상 회로: Blue Pill 보드의 **PA5 → 220Ω → LED → GND** |

### 실행 순서

1. VS Code에 **Wokwi Simulator** 확장을 설치하고, 안내에 따라 Wokwi 계정으로 라이선스를 활성화한다.
2. 이 폴더(`02_BlinkHAL`)를 VS Code 작업 폴더로 연다. `wokwi.toml`이 작업 폴더 최상위에 있어야 한다.
3. PlatformIO **Build**(✓) 또는 `pio run`으로 `firmware.elf`를 만든다. Wokwi는 직접 컴파일하지 않으므로 코드를 고칠 때마다 다시 빌드한다.
4. `F1` → **Wokwi: Start Simulator**. 회로도의 초록 LED가 1초 간격으로 켜졌다 꺼지면 성공이다.
5. `diagram.json`을 열면 회로 편집기가 뜬다. 부품을 추가하거나 배선을 바꿀 수 있다.

### Nucleo-F103RB와 다른 점

Wokwi에는 Nucleo-F103RB 보드가 없어서 같은 STM32F103 계열의 **Blue Pill(STM32F103C8)** 을 대신 쓴다.

- **칩**: F103C8은 Flash 64KB, SRAM 20KB로 F103RB(Flash 128KB)보다 Flash가 작다. 이 예제의 펌웨어는 약 2.7KB라 그대로 들어간다. 같은 medium-density 계열이라 GPIOA·RCC·SysTick 레지스터 구성이 같다.
- **LED**: Blue Pill 보드의 내장 LED는 PC13에 있다. 이 예제는 Nucleo의 LD2와 같은 **PA5**를 쓰므로, 회로도에 PA5용 외부 LED를 따로 달았다. PA5가 High일 때 LED가 켜진다(LD2와 같은 극성).
- **클럭**: 이 예제는 외부 크리스털(HSE)을 쓰지 않고 **HSI 8MHz/2 × PLL16 = 64MHz**를 쓴다. 그래서 보드의 크리스털 유무와 상관없이 같은 클럭 설정이 적용된다.
- **시뮬레이터 범위**: Wokwi의 Blue Pill은 GPIO, RCC, SysTick, EXTI, USART, TIM1~4 등을 시뮬레이션한다. 그러나 DMA, RTC, PWR, IWDG는 지원하지 않는다([Wokwi 문서](https://docs.wokwi.com/parts/board-stm32-bluepill)). 같은 방식을 다른 예제에 쓰려면 먼저 이 범위를 확인한다.
- 실제 보드의 전기적 특성(출력 전류, 엣지 속도, 타이밍 오차)은 시뮬레이션되지 않는다. 최종 확인은 실제 보드로 한다.

## 관련 예제

같은 동작의 [01_BlinkArduino](../01_BlinkArduino/README.md)(더 추상적), [03_BlinkLL](../03_BlinkLL/README.md), [04_BlinkReg](../04_BlinkReg/README.md)(더 낮은 수준)와 비교한다.

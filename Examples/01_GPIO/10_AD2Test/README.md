# 10_AD2Test

> 계층: **HAL** (STM32CubeMX 생성 프로젝트, PlatformIO `framework = stm32cube`)

Nucleo-F103RB의 Arduino 헤더 핀 14개(D2~D13, A4, A5)를 **하나씩 차례로 1초씩 켜고**, 한 바퀴가 끝날 때마다 USART2(PA2/D1)로 `Hello, AD2`를 보낸다.
핀마다 펄스가 순서대로 나오므로, 외부 계측기(예: Digilent Analog Discovery 2 같은 로직 분석기/패턴 생성 확인용)로 **헤더 핀 배선과 핀 매핑을 한눈에 검증**하는 용도의 예제다. (폴더 이름의 "AD2"는 이 용도를 가리키는 것으로 보이며, 코드에는 별도 설명이 없다.)

> 원래 레지스터(CMSIS) 직접 제어로 작성했던 예제를 **HAL + CubeMX 구조로 옮긴 것**이다.
> [04_BlinkReg](../04_BlinkReg/README.md)(레지스터로 PA5 한 핀 점멸)의 확장판이었던 코드를 **핀 표(배열) + 반복문**으로 HAL API에 맞춰 일반화했다.

## 동작

```
main():  HAL_Init() → SystemClock_Config() → MX_GPIO_Init() → MX_USART2_UART_Init()
loop  :  for 각 핀(D2 … D13, A4, A5)
             HAL_GPIO_WritePin(SET) → HAL_Delay(1000) → HAL_GPIO_WritePin(RESET)   (다음 핀으로 바로 이동)
         HAL_UART_Transmit("Hello, AD2\r\n")
```

- 한 바퀴 시간 ≈ 핀 14개 × 1000ms + 메시지 송신 약 1.04ms(12글자 × 10비트 / 115200bps) ≈ **14초**.
- D13(PA5)은 온보드 LD2와 같은 핀이다. 1초 동안 켜지므로 LED로도 순서를 눈으로 확인할 수 있다.
- 로직 분석기로 빠른 펄스를 보고 싶다면 `HAL_Delay()` 값을 줄인다 (실습 과제 1).

## 핀 매핑 (`arduino_pins[]` 순서)

CubeMX에서 각 핀에 라벨을 붙여 `main.h`에 `D2_Pin` / `D2_GPIO_Port` 형태의 정의가 생성되고, `main.c`의 `arduino_pins[]`가 이를 사용한다.

| 순서 | Arduino | MCU 핀 | CubeMX 라벨 | 비고 |
|---|---|---|---|---|
| 1 | D2 | PA10 | `D2` | |
| 2 | D3 | PB3 | `D3` | JTAG(JTDO) 핀 → SWJ 설정 필요 |
| 3 | D4 | PB5 | `D4` | |
| 4 | D5 | PB4 | `D5` | JTAG(NJTRST) 핀 → SWJ 설정 필요 |
| 5 | D6 | PB10 | `D6` | |
| 6 | D7 | PA8 | `D7` | |
| 7 | D8 | PA9 | `D8` | |
| 8 | D9 | PC7 | `D9` | |
| 9 | D10 | PB6 | `D10` | |
| 10 | D11 | PA7 | `D11` | |
| 11 | D12 | PA6 | `D12` | |
| 12 | D13 | PA5 | `LD2` | 온보드 LD2 |
| 13 | A4 | PC1 | `A4` | 아날로그 헤더 |
| 14 | A5 | PC0 | `A5` | 아날로그 헤더 |

**제외한 핀**

- **D0(PA3) / D1(PA2)**: USART2 RX/TX와 겹친다. 이 예제는 D1(PA2)을 GPIO가 아니라 **USART2_TX**로 쓴다. CubeMX가 USART2를 Asynchronous로 설정하면 PA3(RX)도 함께 대체 기능으로 설정되지만 코드에서는 수신하지 않는다.
- **D14(PB9) / D15(PB8)**: I2C 핀(SDA/SCL)이라 사용하지 않는다. 이 보드에서 D14/D15(PB9/PB8)와 A4/A5(PC1/PC0)는 서로 다른 핀이다.

## 핵심 학습 내용

- **CubeMX 생성 코드와 `USER CODE` 영역**: 핀/클럭/UART 초기화(`MX_GPIO_Init`, `MX_USART2_UART_Init`, `SystemClock_Config`)는 CubeMX가 생성한다. 순차 출력 루프와 `arduino_pins[]` 표는 `USER CODE BEGIN PV` / `USER CODE BEGIN 3` 안에 있어 **CubeMX로 코드를 다시 생성해도 보존**된다.
- **핀 표 + 반복문으로 HAL 접근 일반화**: `GpioPin {port, pin}` 배열을 돌며 `HAL_GPIO_WritePin()`으로 모든 핀을 같은 코드로 제어한다. HAL 핀 값은 번호가 아니라 **비트 마스크**(`GPIO_PIN_x = 1 << n`)다.
- **초기 상태 LOW**: `MX_GPIO_Init()`이 `HAL_GPIO_Init()`(→ `GPIOx_CRL/CRH`) 전에 `HAL_GPIO_WritePin(RESET)`(→ `GPIOx_BSRR`)을 먼저 호출해, 출력으로 바뀌는 순간 핀이 HIGH로 튀지 않게 한다.
- **JTAG 핀 해방 (`AFIO_MAPR.SWJ_CFG[2:0]`)**: PB3/PB4는 리셋 직후 JTAG 용도로 점유되어 있다. CubeMX에서 SYS의 Debug를 **Serial Wire**로 두면 `HAL_MspInit()`(`stm32f1xx_hal_msp.c`)에 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`가 생성되어 **JTAG-DP만 끄고 SW-DP(ST-LINK 디버깅)는 유지**한다. 이 매크로 호출 전에 `__HAL_RCC_AFIO_CLK_ENABLE()`(`RCC_APB2ENR.AFIOEN`)도 함께 생성된다.
- **USART2 HAL 초기화**
  1. `MX_USART2_UART_Init()`에서 `huart2.Init`(115200, 8N1, TX/RX)를 채우고 `HAL_UART_Init()` 호출. `USART2->BRR`은 HAL이 PCLK1 기준으로 계산한다.
  2. `HAL_UART_Init()`이 내부에서 **`HAL_UART_MspInit()` 콜백**(`stm32f1xx_hal_msp.c`)을 호출한다. 여기서 `__HAL_RCC_USART2_CLK_ENABLE()`(`RCC_APB1ENR.USART2EN`)과 PA2/PA3의 `GPIO_MODE_AF_PP` 설정이 이루어진다.
- **송신 폴링**: `HAL_UART_Transmit()`이 `SR.TXE`/`SR.TC` 플래그 대기를 내부에서 처리한다. 인터럽트는 쓰지 않는다.
- **`HAL_Delay()`와 SysTick**: `HAL_Init()`이 SysTick을 1ms로 설정하고, `SysTick_Handler`(`stm32f1xx_it.c`)가 `HAL_IncTick()`으로 `uwTick`을 증가시킨다. `HAL_Delay()`는 이 값을 기준으로 대기한다.

### 클럭

CubeMX가 생성한 `SystemClock_Config()`는 **HSI → PLL ×16 = SYSCLK 64MHz**, `APB1 = /2 (32MHz)`, `APB2 = 64MHz`로 설정한다 (다른 CubeMX 예제와 동일). 레지스터 버전은 리셋 기본값 HSI 8MHz를 그대로 썼지만, HAL 버전은 클럭이 올라가 있다.

- UART 보레이트 분주값은 HAL이 PCLK1(32MHz) 기준으로 자동 계산하므로 보레이트는 여전히 **115200**이다.
- 클럭을 바꾸고 싶으면 `.ioc`의 Clock Configuration에서 수정한 뒤 코드를 재생성한다.

## ISR / 콜백

| 구분 | 이름 | 위치 | 역할 |
|---|---|---|---|
| ISR | `SysTick_Handler` | `stm32f1xx_it.c` | 1ms마다 `HAL_IncTick()` 호출 → `uwTick` 증가 |
| HAL 콜백 | `HAL_UART_MspInit` | `stm32f1xx_hal_msp.c` | `HAL_UART_Init()` 안에서 호출되어 USART2 클럭·핀 설정 |
| HAL 콜백 | `HAL_MspInit` | `stm32f1xx_hal_msp.c` | `HAL_Init()` 안에서 호출되어 AFIO/PWR 클럭, SWJ 설정 |

USART2는 인터럽트 없이 폴링으로 보내므로 `USART2_IRQHandler`는 없다.

## 하드웨어 / 확인 방법

- NUCLEO-F103RB 보드, USB 케이블 (ST-LINK).
- **시리얼 출력**: D1(PA2)은 ST-LINK 가상 COM 포트로도 연결되어 있다. 이 예제의 보드레이트는 **115200**이며 `platformio.ini`의 `monitor_speed = 115200`과 일치한다. `monitor_port = COM9`는 작성자 PC 기준이다.
- **로직 분석기/오실로스코프**: D2~D13, A4, A5에 채널을 연결하면 핀이 위 표의 순서대로 하나씩 1초 펄스를 내는 것을 볼 수 있다. D1(PA2)에서는 UART 프레임 (`Hello, AD2\r\n`, 115200 8N1)을 디코드할 수 있다.
- 한 번에 한 핀만 HIGH이므로, 배선이 서로 단락되었거나 핀 번호가 어긋난 경우 다른 채널에 같은 펄스가 함께 나타나서 구별된다.

## 폴더 구성

```
10_AD2Test/
├─ AD2Test.ioc                  ← CubeMX 설정 (핀, USART2, 클럭, SYS)
├─ Core/
│  ├─ Inc/                      ← main.h(핀 라벨 정의), stm32f1xx_hal_conf.h, stm32f1xx_it.h
│  ├─ Src/main.c                ← 이 예제의 소스 (USER CODE 영역에 순차 출력 루프)
│  ├─ Src/stm32f1xx_it.c        ← SysTick_Handler 등 예외/인터럽트 핸들러
│  ├─ Src/stm32f1xx_hal_msp.c   ← HAL_MspInit, HAL_UART_MspInit (클럭·핀·SWJ)
│  └─ Startup/                  ← 시작 코드 (벡터 테이블)
├─ Drivers/                     ← HAL/CMSIS 드라이버 (CubeMX가 복사, git 제외)
├─ STM32F103RBTX_FLASH.ld       ← 링커 스크립트
├─ platformio.ini               ← framework = stm32cube, src_dir = Core/Src
└─ STM32F103R_ArduinoPinmap.pdsprj   ← Proteus 시뮬레이션 프로젝트
```

CubeMX가 재생성할 때 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 바깥은 덮어써지므로, 사용자 코드는 반드시 그 영역 안에 둔다.

## 참고 레지스터

HAL이 내부에서 다루는 레지스터다 (직접 접근하지 않는다).

| 레지스터 | 관련 HAL 호출 | 문서 · 절 |
|---|---|---|
| `RCC_APB2ENR` (`IOPAEN`, `IOPBEN`, `IOPCEN`, `AFIOEN`) | `__HAL_RCC_GPIOx_CLK_ENABLE()`, `__HAL_RCC_AFIO_CLK_ENABLE()` | [RM0008](../../../Docs/RM0008_Reference_manual.pdf) 7.3.7 (p.112) |
| `RCC_APB1ENR` (`USART2EN`) | `__HAL_RCC_USART2_CLK_ENABLE()` | RM0008 7.3.8 |
| `AFIO_MAPR` (`SWJ_CFG[2:0]` = `010`) | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` | RM0008 9.4.2 |
| `GPIOx_CRL` / `GPIOx_CRH` | `HAL_GPIO_Init()` | RM0008 9.2.1 (p.171) / 9.2.2, Table 20·21 (p.161) |
| `GPIOx_BSRR` | `HAL_GPIO_WritePin()` | RM0008 9.2.5 (p.173) |
| `USART_SR` / `DR` / `BRR` / `CR1` | `HAL_UART_Init()`, `HAL_UART_Transmit()` | RM0008 27.6 USART registers |
| `STK_CTRL` / `STK_LOAD` / `STK_VAL` | `HAL_Init()` → `HAL_SYSTICK_Config()` | [PM0056](<../../../Docs/PM0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf>) 4.5 (p.151~153) |

Arduino 핀과 MCU 핀의 대응은 보드 배선이므로 [UM1724](../../../Docs/UM1724_User_Manual_STM32Nucleo64.pdf)의 Arduino 커넥터 표와 회로도에서 확인한다.

## 실습 과제

1. **펄스 폭 바꾸기**: `HAL_Delay(1000)`을 `HAL_Delay(1)`로 바꿔 로직 분석기에서 약 1ms 펄스가 순서대로 나오는지 확인한다. 한 바퀴 시간이 어떻게 바뀌는지 계산해 본다.
2. **D14/D15 추가**: CubeMX에서 PB9, PB8을 `GPIO_Output`으로 추가하고 `arduino_pins[]`에 넣는다. 핀 14개가 16개가 되고 한 바퀴 시간이 어떻게 달라지는지 계산해 본다.
3. **SWJ 설정 실험**: CubeMX에서 SYS Debug를 JTAG+SWD(Full SWJ)로 바꿔 재생성하면 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`가 사라진다. D3(PB3)/D5(PB4)에서 펄스가 나오는지 확인하고, 디버거의 `AFIO > MAPR`로 값이 어떻게 달라지는지 본다.
4. **보드레이트 바꾸기**: `huart2.Init.BaudRate`를 9600으로 바꾸고 `platformio.ini`의 `monitor_speed`도 맞춘다. 실제 보드레이트 오차를 `USART2->BRR` 값으로 계산해 본다.
5. **송신 인터럽트 방식으로 바꾸기**: `HAL_UART_Transmit()`을 `HAL_UART_Transmit_IT()`로 바꾸고 CubeMX에서 USART2 global interrupt를 켠 뒤, `HAL_UART_TxCpltCallback()`으로 완료를 확인한다. 폴링 방식과 CPU 대기 시간을 비교한다.

## 관련 예제

- [04_BlinkReg](../04_BlinkReg/README.md): 레지스터 방식의 단일 핀 점멸. 이 예제의 원래 출발점.
- [08_ExternalInt](../08_ExternalInt/README.md): 같은 CubeMX + HAL 구조에서 EXTI를 다루는 예제.
- [08_Clock_System/07_AFIO_Remap_Reg_c](../../08_Clock_System/07_AFIO_Remap_Reg_c/README.md): `AFIO_MAPR`로 JTAG 핀 해방과 핀 리맵.
- [02_USART/01_USART_printf](../../02_USART/01_USART_printf/README.md): HAL로 하는 USART2 송신.

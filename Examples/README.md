# Examples — STM32 Nucleo-F103RB 실습 예제

Nucleo-F103RB(Cortex-M3, 64MHz, Flash 128KB / SRAM 20KB) 보드로 GPIO부터 통신·타이머·저전력·CAN까지 단계별로 익히는 예제 모음이다.
각 예제 폴더에 자체 `README.md`가 있으며, 이 문서는 전체 지도와 학습 순서를 안내한다.

## 폴더 구조

| 그룹 | 주제 | 핵심 키워드 |
|---|---|---|
| [01_GPIO](01_GPIO/README.md) | 출력·입력·외부 인터럽트 | 4가지 추상화 계층 비교, EXTI, 비트 밴딩 |
| [02_USART](02_USART/README.md) | 시리얼 통신 | printf 리다이렉션, 수신 인터럽트, DMA+IDLE, 링 버퍼, CLI, 단선 반이중 |
| [03_ADC](03_ADC/README.md) | 아날로그 입력 | 타이머 트리거, DMA, 아날로그 워치독, Vrefint 보정, 주입 채널, 듀얼 모드, 가변저항 PWM |
| [04_I2C_Communication](04_I2C_Communication/README.md) | I2C | 주소 스캔, 문자 LCD, MPU6050(폴링/DMA), 슬레이브 모드, OLED, EEPROM, DS3231 RTC |
| [05_SPI_Communication](05_SPI_Communication/README.md) | SPI | 루프백, 전이중 DMA, 슬레이브 모드, OLED, SD 카드, NOR Flash, MAX7219 |
| [06_Timers_RTC](06_Timers_RTC/README.md) | 타이머·RTC | 시간 기준, PWM, 입력 캡처, 데드타임, RTC, PWM 입력, 엔코더, 원 펄스, 서보, 32비트 연결, 모터 드라이버 |
| [07_DMA](07_DMA/README.md) | DMA | 메모리↔메모리, 원형 DMA 핑퐁, 채널 우선순위 |
| [08_Clock_System](08_Clock_System/README.md) | 클럭·코어 시스템 | RCC, HSE+PLL 72MHz, SysTick, NVIC, AFIO, 폴트, PendSV, MCO, DWT, ITM/SWO, 상태 머신, 스택 워터마크 |
| [09_WatchDog_Sleep](09_WatchDog_Sleep/README.md) | 워치독·저전력 | WWDG, IWDG, Sleep/Stop/Standby, BKP 탬퍼 |
| [10_Flash_CRC](10_Flash_CRC/README.md) | 내부 Flash·CRC·부트 | 페이지 소거/쓰기, 하드웨어 CRC, IAP 부트로더/앱, XMODEM 업데이트, 옵션 바이트 |
| [11_CAN_Communication](11_CAN_Communication/README.md) | CAN | bxCAN 루프백, 수신 필터, 실제 버스(2보드) |

## 명명 규칙

- 그룹 폴더는 `01_GPIO`, `02_USART`… 처럼 번호를 붙이고, **예제 폴더 번호는 그룹 안에서 항상 `02_BlinkHAL`부터 순서대로** 붙인다 (예: `08_Clock_System/01_Clock_Config_Reg_c`). 예제를 추가하면 그 그룹의 다음 번호를 쓴다.
- 폴더명 끝의 **`_c`**는 Claude가 작성한 예제(사용자 검토 전)이다. 이름의 계층 표기(`_HAL`, `_LL`, `_Reg`)가 그 예제의 추상화 수준이다.
- 모든 예제는 소스 상단 주석에 계층, 사용 레지스터·비트, ISR과 HAL 콜백의 역할 구분을 적어 두었다. README는 그 요약이며 세부 내용은 `main.c` 주석을 참고한다.

## 추상화 계층

같은 기능도 계층에 따라 코드가 크게 달라진다. 01_GPIO의 Blink 4종(`01_BlinkArduino`~`04_BlinkReg`)이 이를 직접 비교하도록 만든 출발점이다.

| 계층 | 특징 | 대표 예제 |
|---|---|---|
| Arduino | 가장 단순, 하드웨어가 숨겨짐 | `01_BlinkArduino` |
| HAL | 이식성·생산성, CubeMX 코드 생성 | `02_BlinkHAL`, 대부분의 `_HAL_c` |
| LL | 레지스터에 가까운 얇은 래퍼 | `03_BlinkLL` |
| 레지스터(CMSIS) | 비트 단위 직접 제어, 동작 원리 학습 | `04_BlinkReg`, `_Reg_c` |

## 개발·실행 방법

- VS Code + PlatformIO: 폴더를 열어 Build / Upload (`platformio.ini`에 `framework = stm32cube`, `cmsis`, `arduino` 중 하나가 지정됨)
- 일부 예제는 STM32CubeIDE 프로젝트(`.ioc`, `.project`, `.cproject`)도 포함한다. CubeMX 재생성 시 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 바깥은 덮어써진다.
- 시리얼 출력 예제는 ST-Link 가상 COM 포트(USART2, PA2=TX / PA3=RX)를 사용한다. 대부분 115200 8N1이며, `01_USART_printf`는 9600이다.

## Nucleo-F103RB 공통 핀

| 기능 | 핀 |
|---|---|
| 사용자 LED LD2 | PA5 (Arduino D13) |
| 사용자 버튼 B1 | PC13 (눌림 = Low, 보드에 풀업) |
| ST-Link VCP | USART2: PA2(TX), PA3(RX) |
| SWD | PA13, PA14 |
| 시스템 클럭 | HSE는 ST-Link MCO(바이패스)라 대부분 HSI/2 × 16 = 64MHz(PLL) 사용 |

## 권장 학습 순서

1. **GPIO** `01_GPIO/01`~`04`: 같은 Blink를 4계층으로 비교 → `05`~`08` 입력/EXTI → `09` 비트 밴딩
2. **USART** `02_USART/01`~`06`: printf 출력으로 디버깅 수단 확보 → 인터럽트·DMA·링 버퍼·CLI·단선
3. **Clock/SysTick/NVIC** `08_Clock_System/01`~`03`: 클럭 트리와 인터럽트 우선순위 이해
4. **Timer** `06_Timers_RTC/01`~`11`, **ADC** `03_ADC/01`~`07`: 주기·PWM·측정
5. **통신** I2C(`04_I2C_Communication/01`~`08`), SPI(`05_SPI_Communication/01`~`07`), CAN(`11_CAN_Communication/01`~`03`)
6. **DMA** `07_DMA/01`~`03`, **저전력** `09_WatchDog_Sleep/01`~`07`, **Flash/CRC/부트** `10_Flash_CRC/01`~`06`
7. **심화** `08_Clock_System/04`~`13`: AFIO, HardFault 진단, PendSV 문맥 교환, MCO, DWT, 상태 머신, 칩 정보, HSE 72MHz, ITM/SWO, 스택 워터마크

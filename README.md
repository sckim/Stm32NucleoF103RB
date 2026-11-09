# Stm32NucleoF103RB

**임베디드시스템** 교과목의 실습 자료 저장소이다. STM32 Nucleo-F103RB(ARM Cortex-M3) 보드를 기준으로 GPIO부터 인터럽트, 타이머, ADC, UART, I2C까지 단계별 예제 프로젝트와 참고 문서, 회로 시뮬레이션 파일을 정리한다.

## 개발 환경

* IDE: [VS Code](https://code.visualstudio.com/) + [PlatformIO](https://platformio.org/) (예제 대부분), 일부는 STM32CubeIDE 프로젝트 파일(`.project`, `.cproject`, `.ioc`)을 함께 포함함
* 보드 설정: [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)로 생성한 `.ioc` 파일 기준
* 드라이버: STM32 HAL 및 LL(Low-Layer) 라이브러리, 일부 예제는 레지스터 직접 제어 방식
* 회로 시뮬레이션: [Proteus](https://www.labcenter.com/) (프로젝트 파일 `STM32F103R_ArduinoPinmap.pdsprj`는 각 예제 폴더에 있다). 파일이름은 같지만 내부 회로도가 조금씩 다를 수 있다.

## 폴더 구조

| 폴더/파일 | 내용 |
|---|---|
| `Docs/` | 데이터시트, 레퍼런스 매뉴얼 등 STM32F103 관련 공식 문서(PDF) |
| `Examples/` | 실습 예제 프로젝트 모음 |
| `Schematics/AD2_extension/` | AD2 확장 키트 KiCad 회로도/PCB ([README](Schematics/AD2_extension/README.md)) — 2026-10 `01_Lectures` 저장소의 `Schematics/AD2_extension`에서 이동 |

## 문서 활용법

이 저장소는 예제 코드와 함께 여러 층의 문서를 제공한다. 어디에 무엇이 있는지, 무엇을 먼저 여는지를 이 장에서 정리한다.

### 1. 실습을 시작하기 전: 저장소 받기와 최신화

| 문서 | 내용 | 언제 보나 |
|---|---|---|
| [Examples/실습자료_최신화.pdf](Examples/실습자료_최신화.pdf) | VS Code에서 저장소를 클론하고, 수업 전에 Pull로 최신 자료를 받는 절차. 자주 만나는 상황표와 수업 전 체크리스트 | 처음 한 번, 그리고 매 수업 시작 전 |
| [Git_개념과_기본사용법.pdf](Git_개념과_기본사용법.pdf) | Git과 GitHub를 쓰는 이유, Git의 세 작업 공간, 커밋과 GitHub에 올리기, Clone·Pull·Fetch의 차이, 브랜치, 자주 쓰는 명령어, 수업에서 지킬 기본 습관 | 위 절차의 용어가 낯설 때, 내 변경을 기록하고 싶을 때 |

학생은 자료를 내려받기만 한다. 수업에서 지시하지 않았다면 Push나 Sync Changes는 누르지 않는다. 오류나 충돌이 나오면 파일을 지우지 말고 담당자에게 문의한다(자세한 상황표는 `실습자료_최신화.pdf` 4절).

### 2. 문서의 층: 어디에 무엇이 있는가

| 위치 | 문서 | 용도 |
|---|---|---|
| 저장소 루트 | 이 README | 저장소 전체 안내, 예제 목록, 참고 문서, 시뮬레이션 안내 |
| `Examples/` | [Examples/README.md](Examples/README.md) | 예제 전체 지도, 학습 순서, 명명 규칙 |
| 그룹 폴더 (예: `Examples/03_ADC/`) | **`<그룹명> 교안.html` (상위 교안)** | 그 주변장치가 칩 안에서 어떻게 동작하는지를 원본 블록도와 신호 경로로 이해한다. 그룹별 링크는 아래 [그룹별 상위 교안](#그룹별-상위-교안)에 있다 |
| 그룹 폴더 | `README.md` | 그룹의 예제 목록, 공통 배선·핀, 배우는 것 |
| 예제 폴더 | `<예제명> 교안.html` (예제 교안) | 그 예제의 코드를 레지스터·비트 단위로 해석하고, 시뮬레이터·실험·확인 문제로 확인한다. 아직 없는 예제도 있다 |
| 예제 폴더 | `README.md`, `main.c` 상단 주석 | 요약, 사용 레지스터·비트, ISR과 HAL 콜백의 역할 구분 |
| 예제 폴더 | `*.pdsprj`, (`Examples/01_GPIO/02_BlinkHAL`) `wokwi.toml`·`diagram.json` | 보드 없이 시뮬레이션한다. 설정과 제약은 아래 [회로 시뮬레이션](#회로-시뮬레이션) 참고 |
| `Docs/` | RM0008, 데이터시트 등 원본 PDF | 교안이 인용한 쪽을 직접 확인한다. [Docs/README.md](Docs/README.md)에 읽는 순서가 있다 |

### 3. 그룹 하나를 공부하는 순서 (권장)

1. **그룹 `README.md`**: 예제 목록과 필요한 배선을 확인한다.
2. **상위 교안 1~3장**: 문제 정의, 일반 개념, F103 내부 블록도를 먼저 읽는다. 개념과 신호의 흐름을 먼저 잡는다.
3. **예제 교안과 코드**: 예제를 번호 순서대로 빌드하고 실행한다. 예제 교안이 없는 예제는 예제 README와 `main.c` 주석을 본다.
4. **상위 교안 4~7장**: 클럭·시간 계산, 제어 요소, 이벤트, 동작 시나리오를 실습한 예제와 연결해 다시 읽는다.
5. **상위 교안 9장 확인 문제**로 점검한다. 답은 접혀 있으므로 먼저 풀어 본 뒤 연다.
6. **막히면** 상위 교안 8장(설계상 한계와 정오표)을 보고, 그래도 모르면 `Docs/`의 원본 PDF로 간다.

상위 교안 10장의 표는 "각 예제가 상위 교안의 어느 장을 쓰는지"를 보여 준다. 예제를 하다가 개념이 막히면 이 표에서 해당 장으로 돌아간다.

### 4. 상위 교안은 이렇게 읽는다

상위 교안은 모든 그룹이 같은 10장 틀이다.

| 장 | 내용 | 이럴 때 본다 |
|---|---|---|
| 1 | 문제 정의 | 이 주변장치가 없으면 CPU가 무엇을 직접 해야 하는지 |
| 2 | 칩과 무관한 일반 개념 | 디지털논리와 연결되는 기본 원리 |
| 3 | F103 내부 블록도 | 신호가 핀에서 CPU까지 어떤 경로로 흐르는지 |
| 4 | 클럭 경로와 시간 계산 | 주파수·시간을 계산할 때(계산기 포함) |
| 5 | 제어 요소의 역할 | 레지스터 비트가 블록도의 어느 스위치인지 |
| 6 | 이벤트 생성 기전 | 플래그, 인터럽트, DMA 요청, 다른 주변장치 연결 |
| 7 | 동작 시나리오 | 한 번의 동작을 시간 순서의 타이밍도로 따라갈 때 |
| 8 | 설계상 한계와 이유 | 왜 이렇게 만들었는지, 정오표의 알려진 결함 |
| 9 | 이해 확인 문제 | 계산형·추론형 문제로 점검할 때 |
| 10 | 하부 예제 연결 | 어떤 예제가 이 경로를 쓰는지, 마지막 "정리" 상자 |

읽을 때 알아 둘 점은 다음과 같다.

- **브라우저로 연다.** `.html` 파일을 더블클릭하면 된다. 그림은 같은 그룹 폴더의 `img/`에, PDF 링크는 `Docs/`를 가리키므로 저장소 폴더 구조를 그대로 둔다.
- **그림 캡션의 "출처" 링크**를 누르면 원본 PDF의 그 쪽이 열린다. 교안의 모든 사실은 이렇게 문서의 쪽과 Figure·Table 번호로 확인할 수 있다.
- **"등가 모델" 표시**는 문서에 내부 구현이 없어 디지털논리 소자(카운터, 비교기, 시프트 레지스터 등)로 바꿔 그린 설명이라는 뜻이다. 실제 회로도가 아니다.
- **약어**는 처음 나올 때 풀어 쓰고, 앞쪽의 접힌 "약어 모음"에도 모아 두었다.
- **코드와 라이브러리 함수는 상위 교안에 없다.** 코드는 예제 교안과 `main.c`에서 다룬다.
- `Examples/12_Advanced_Topics`에는 상위 교안이 없다.

## 실습 예제 (`Examples/`)

### 그룹별 상위 교안

각 그룹의 주변장치 하드웨어를 원본 블록도와 디지털논리 관점으로 설명하는 교안이다(같은 10장 틀). 예제별 교안은 각 예제 폴더에 있다.

| 상위 교안 |
|---|
| [01_GPIO](Examples/01_GPIO/01_GPIO%20교안.html) |
| [02_USART](Examples/02_USART/02_USART%20교안.html) |
| [03_ADC](Examples/03_ADC/03_ADC%20교안.html) |
| [04_I2C_Communication](Examples/04_I2C_Communication/04_I2C_Communication%20교안.html) |
| [05_SPI_Communication](Examples/05_SPI_Communication/05_SPI_Communication%20교안.html) |
| [06_Timers_RTC](Examples/06_Timers_RTC/06_Timers_RTC%20교안.html) |
| [07_DMA](Examples/07_DMA/07_DMA%20교안.html) |
| [08_Clock_System](Examples/08_Clock_System/08_Clock_System%20교안.html) |
| [09_WatchDog_Sleep](Examples/09_WatchDog_Sleep/09_WatchDog_Sleep%20교안.html) |
| [10_Flash_CRC](Examples/10_Flash_CRC/10_Flash_CRC%20교안.html) |
| [11_CAN_Communication](Examples/11_CAN_Communication/11_CAN_Communication%20교안.html) |

### 예제 목록

| 번호 | 예제 (`그룹/예제`) | 구현 방식 | 내용 |
|---|---|---|---|
| 01 | [`01_GPIO/01_BlinkArduino`](Examples/01_GPIO/01_BlinkArduino) | Arduino 프레임워크(PlatformIO) | `pinMode`/`digitalWrite`로 온보드 LED(PA5) 점멸 |
| 02 | [`01_GPIO/02_BlinkHAL`](Examples/01_GPIO/02_BlinkHAL) | HAL 드라이버 | HAL API 기반 LED 점멸 |
| 03 | [`01_GPIO/03_BlinkLL`](Examples/01_GPIO/03_BlinkLL) | LL 드라이버 | LL API 기반 LED 점멸 |
| 04 | [`01_GPIO/04_BlinkReg`](Examples/01_GPIO/04_BlinkReg) | 레지스터 직접 제어 | `RCC`, `GPIOx->CRL` 레지스터를 직접 조작하여 LED 점멸, SysTick 기반 딜레이 구현 |
| 05 | [`01_GPIO/05_ReadPin`](Examples/01_GPIO/05_ReadPin) | HAL | 사용자 버튼(B1) 폴링 입력으로 LED(LD2) 제어 |
| 06 | [`01_GPIO/06_ExtInt`](Examples/01_GPIO/06_ExtInt) | HAL, EXTI | 버튼 입력을 EXTI 인터럽트로 처리(`HAL_GPIO_EXTI_Callback`) |
| 07 | [`01_GPIO/07_ReadPinReg`](Examples/01_GPIO/07_ReadPinReg) | 레지스터 | `GPIOC->IDR` 직접 읽기로 버튼 폴링 (05_ReadPin의 레지스터 버전) |
| 08 | [`01_GPIO/08_ExternalInt`](Examples/01_GPIO/08_ExternalInt) | HAL, EXTI | 두 개 EXTI 라인(`EXTI9_5`, `EXTI15_10`)의 버튼 B1(PC13)·B2(PA6)를 하나의 `HAL_GPIO_EXTI_Callback`에서 구분 처리 |
| 09 | [`01_GPIO/09_BitBandingReg`](Examples/01_GPIO/09_BitBandingReg) | 레지스터 | Cortex-M3 비트 밴딩 별칭으로 RCC/GPIO/SRAM 비트를 원자적으로 접근 |
| 01 | [`02_USART/01_USART_printf`](Examples/02_USART/01_USART_printf) | HAL, UART | `printf` 출력을 USART로 리다이렉션 |
| 02 | [`02_USART/02_USART_Interrupt`](Examples/02_USART/02_USART_Interrupt) | HAL, UART, NVIC | USART2 수신 인터럽트(`HAL_UART_RxCpltCallback`)로 에코 및 LED 제어 |
| 03 | [`02_USART/03_USART_RingBuffer`](Examples/02_USART/03_USART_RingBuffer) | HAL, UART | ISR->main 링 버퍼(lock-free), 오버플로 시험(B1로 main 정지) |
| 04 | [`02_USART/04_USART_DMA_IdleLine_c`](Examples/02_USART/04_USART_DMA_IdleLine_c) | HAL, UART, DMA | (검토 전) DMA 원형 수신 + IDLE 라인 감지로 가변 길이 패킷 처리 |
| 05 | [`02_USART/05_UART_CLI_c`](Examples/02_USART/05_UART_CLI_c) | HAL, UART | (검토 전) 명령줄 인터페이스: 줄 편집, 명령 테이블(help/led/uptime/echo/peek/reset) |
| 06 | [`02_USART/06_USART_HalfDuplex_c`](Examples/02_USART/06_USART_HalfDuplex_c) | HAL, UART | (검토 전) USART 단선 반이중(PA9 한 선), 방향 전환, 오픈드레인 버스 |
| 07 | [`02_USART/07_USART_Stack`](Examples/02_USART/07_USART_Stack) | HAL, UART, CMSIS | 스택(LIFO) 괄호 검사기, ISR 공유 시 경쟁 상태와 PRIMASK 임계 구역 |
| 08 | [`02_USART/08_USART_printf_Dual`](Examples/02_USART/08_USART_printf_Dual) | HAL, UART | `printf`를 USART2(VCP)와 USART1(PA9=D8)로 동시 출력, AD2로 UART 파형 관찰 |
| 01 | [`03_ADC/01_ADC_Single`](Examples/03_ADC/01_ADC_Single) | HAL, ADC | 외부 핀 PA0(CH0) 한 채널 ADC 값을 UART로 출력 |
| 02 | [`03_ADC/02_ADC_Temperature`](Examples/03_ADC/02_ADC_Temperature) | HAL, ADC | 내부 온도 센서(CH16) 값을 섭씨로 환산해 UART로 출력 |
| 03 | [`03_ADC/03_ADC_Multi`](Examples/03_ADC/03_ADC_Multi) | HAL, ADC | 스캔 + 불연속 모드로 DMA 없이 4채널(A0/A1/A2/Vrefint) 폴링 |
| 04 | [`03_ADC/04_ADC_DMA_TimTrigger`](Examples/03_ADC/04_ADC_DMA_TimTrigger) | HAL, ADC, DMA, TIM | TIM3 TRGO 트리거 1kHz 2채널 스캔, DMA 원형 저장, HT/TC 콜백 |
| 05 | [`03_ADC/05_ADC_AnalogWatchdog_c`](Examples/03_ADC/05_ADC_AnalogWatchdog_c) | HAL, ADC, DMA | (검토 전) ADC 아날로그 워치독으로 전압 창(window) 이탈 감지 인터럽트 |
| 06 | [`03_ADC/06_ADC_Vrefint_VDDA_c`](Examples/03_ADC/06_ADC_Vrefint_VDDA_c) | HAL, ADC | (검토 전) Vrefint 로 실제 VDDA 를 측정해 ADC 값을 보정 |
| 07 | [`03_ADC/07_ADC_Potentiometer_PWM_c`](Examples/03_ADC/07_ADC_Potentiometer_PWM_c) | HAL, ADC, TIM | (검토 전) 가변저항 → 이동평균/히스테리시스/감마 → PWM 밝기 |
| 01 | [`04_I2C_Communication/01_I2C_Scan`](Examples/04_I2C_Communication/01_I2C_Scan) | HAL, I2C | I2C1 버스 스캐너(0x08~0x77), 흔한 장치 이름 표시 |
| 02 | [`04_I2C_Communication/02_PCF8574_Output`](Examples/04_I2C_Communication/02_PCF8574_Output) | HAL, I2C | PCF8574 기본: 1바이트로 핀 8개(P7~P0) 출력, 다시 읽어 비교, 액티브 Low LED |
| 03 | [`04_I2C_Communication/03_PCF8574_LCD`](Examples/04_I2C_Communication/03_PCF8574_LCD) | HAL, I2C | PCF8574 I2C 확장 IC를 통한 문자 LCD 제어, I2C 주소 스캐너 포함 |
| 04 | [`04_I2C_Communication/04_I2C_MPU6050_c`](Examples/04_I2C_Communication/04_I2C_MPU6050_c) | HAL, I2C | (검토 전) MPU6050 WHO_AM_I 확인과 가속도/자이로/온도 14바이트 연속 읽기 |
| 05 | [`04_I2C_Communication/05_I2C_MPU6050_DMA_c`](Examples/04_I2C_Communication/05_I2C_MPU6050_DMA_c) | HAL, I2C, DMA | (검토 전) 논블로킹 I2C(DMA)로 MPU6050 읽기, 오류 복구 |
| 06 | [`04_I2C_Communication/06_I2C_OLED_SSD1306_c`](Examples/04_I2C_Communication/06_I2C_OLED_SSD1306_c) | HAL, I2C | (검토 전) SSD1306 OLED, 프레임버퍼, 선/숫자/애니메이션 |
| 07 | [`04_I2C_Communication/07_I2C_EEPROM_AT24C_c`](Examples/04_I2C_Communication/07_I2C_EEPROM_AT24C_c) | HAL, I2C | (검토 전) AT24Cxx EEPROM: 16비트 주소, 페이지 경계, ACK 폴링 |
| 08 | [`04_I2C_Communication/08_I2C_DS3231_RTC_c`](Examples/04_I2C_Communication/08_I2C_DS3231_RTC_c) | HAL, I2C | (검토 전) 외장 RTC DS3231: BCD 시각, 온도, OSF 플래그 |
| 01 | [`05_SPI_Communication/01_SPI_Loopback_c`](Examples/05_SPI_Communication/01_SPI_Loopback_c) | HAL, SPI | (검토 전) SPI1 마스터 루프백(MOSI-MISO 점퍼), 분주비별 전송 속도 측정 |
| 02 | [`05_SPI_Communication/02_SPI_DMA_Fullduplex_c`](Examples/05_SPI_Communication/02_SPI_DMA_Fullduplex_c) | HAL, SPI, DMA | (검토 전) SPI1 전이중 DMA 전송, 폴링 방식과 시간/CPU 여유 비교 |
| 03 | [`05_SPI_Communication/03_SPI_OLED_SSD1306_c`](Examples/05_SPI_Communication/03_SPI_OLED_SSD1306_c) | HAL, SPI | (검토 전) SPI 로 SSD1306 OLED (D/C 핀, 8MHz) |
| 04 | [`05_SPI_Communication/04_SPI_SDcard_c`](Examples/05_SPI_Communication/04_SPI_SDcard_c) | HAL, SPI | (검토 전) SPI 모드 SD 카드 초기화, 섹터 읽기(MBR), 용량 계산 |
| 05 | [`05_SPI_Communication/05_SPI_Flash_W25Qxx_c`](Examples/05_SPI_Communication/05_SPI_Flash_W25Qxx_c) | HAL, SPI | (검토 전) SPI NOR Flash(W25Qxx): JEDEC ID, 섹터 소거, 페이지 프로그램 |
| 06 | [`05_SPI_Communication/06_SPI_MAX7219_c`](Examples/05_SPI_Communication/06_SPI_MAX7219_c) | HAL, SPI | (검토 전) MAX7219 8x8 LED 매트릭스, 16비트 프레임/LOAD 반영 |
| 01 | [`06_Timers_RTC/01_TIM_TimeBase`](Examples/06_Timers_RTC/01_TIM_TimeBase) | HAL, TIM | 타이머 주기 인터럽트(`HAL_TIM_PeriodElapsedCallback`)로 LED 토글 |
| 02 | [`06_Timers_RTC/02_TIM_TimeBase_Polling`](Examples/06_Timers_RTC/02_TIM_TimeBase_Polling) | HAL, TIM | 01의 폴링 버전: `SR.UIF` 직접 확인·클리어, printf 블로킹으로 인한 틱 누락(lag) 관찰 |
| 03 | [`06_Timers_RTC/03_TIM_Overflow_Counter`](Examples/06_Timers_RTC/03_TIM_Overflow_Counter) | HAL+레지스터, TIM | 오버플로 횟수(`gOvf`)를 상위 16비트로 붙인 32비트 µs 시계, 두 번 읽기 경쟁 조건과 UIF 보정
| 04 | [`06_Timers_RTC/04_TIM_OutputCompare`](Examples/06_Timers_RTC/04_TIM_OutputCompare) | HAL, TIM | 출력 비교 Toggle/Timing 모드, CCR 전진으로 채널별 주파수
| 05 | [`06_Timers_RTC/05_TIM_PWM`](Examples/06_Timers_RTC/05_TIM_PWM) | HAL, TIM | TIM2_CH1(PA0) 1kHz PWM, LED 밝기 왕복 |
| 06 | [`06_Timers_RTC/06_TIM_InputCapture_c`](Examples/06_Timers_RTC/06_TIM_InputCapture_c) | HAL, TIM | (검토 전) 입력 캡처로 주기/주파수 측정 (TIM3 PWM 테스트 신호 포함) |
| 07 | [`06_Timers_RTC/07_TIM_PWMInput_c`](Examples/06_Timers_RTC/07_TIM_PWMInput_c) | HAL, TIM | (검토 전) PWM 입력 모드(슬레이브 리셋)로 주파수/듀티 하드웨어 측정 |
| 08 | [`06_Timers_RTC/08_TIM1_DeadTime_Break_c`](Examples/06_Timers_RTC/08_TIM1_DeadTime_Break_c) | HAL, TIM1 | (검토 전) 상보 PWM + 데드타임(약 1us) + 브레이크 입력/소프트웨어 브레이크 |
| 09 | [`06_Timers_RTC/09_TIM_Encoder_c`](Examples/06_Timers_RTC/09_TIM_Encoder_c) | HAL, TIM | (검토 전) 엔코더 인터페이스 모드(x4), 소프트웨어 직교 신호 시뮬레이션 포함 |
| 10 | [`06_Timers_RTC/10_TIM_OnePulse_HCSR04_c`](Examples/06_Timers_RTC/10_TIM_OnePulse_HCSR04_c) | HAL, TIM | (검토 전) 원 펄스 모드 10us 트리거 + 입력 캡처로 HC-SR04 거리 측정 |
| 11 | [`06_Timers_RTC/11_TIM_Servo_c`](Examples/06_Timers_RTC/11_TIM_Servo_c) | HAL, TIM | (검토 전) RC 서보: 50Hz PWM, 1~2ms = 0~180° |
| 12 | [`06_Timers_RTC/12_TIM_MotorDriver_TB6612_c`](Examples/06_Timers_RTC/12_TIM_MotorDriver_TB6612_c) | HAL, TIM | (검토 전) H-브리지 모터: 20kHz PWM, 쇼트 브레이크/코스트, 비상 정지 |
| 13 | [`06_Timers_RTC/13_RTC_Calendar_c`](Examples/06_Timers_RTC/13_RTC_Calendar_c) | HAL+레지스터, RTC | (검토 전) F1 RTC(32비트 카운터)를 Unix 시간으로 쓰는 달력 시계, 1초 인터럽트 |
| 01 | [`07_DMA/01_DMA_MemToMem`](Examples/07_DMA/01_DMA_MemToMem) | HAL, DMA | 메모리->메모리 DMA 복사, CPU 복사와 DWT 사이클 비교 |
| 02 | [`07_DMA/02_DMA_Circular_GPIO`](Examples/07_DMA/02_DMA_Circular_GPIO) | HAL, DMA, TIM | 원형 DMA + 핑퐁 버퍼로 GPIOC 패턴 자동 출력 |
| 01 | [`08_Clock_System/01_CoreRegisters_Instructions_Reg_c`](Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) | 레지스터+ASM | (검토 전) 레지스터 파일, 조건 플래그, IT 조건부실행, 스택 PUSH/POP, 하드웨어 나눗셈 |
| 02 | [`08_Clock_System/02_MemoryMap_Reg_c`](Examples/08_Clock_System/02_MemoryMap_Reg_c) | 레지스터 | (검토 전) 4GB 주소 공간, 버스 계층, memory-mapped I/O, 리틀 엔디안 |
| 03 | [`08_Clock_System/03_ExceptionModel_VectorTable_Reg_c`](Examples/08_Clock_System/03_ExceptionModel_VectorTable_Reg_c) | 레지스터+ASM | (검토 전) 벡터 테이블, 예외 자동 스택 저장, EXC_RETURN, SVC |
| 04 | [`08_Clock_System/04_Clock_Config_Reg_c`](Examples/08_Clock_System/04_Clock_Config_Reg_c) | 레지스터 | (검토 전) RCC/FLASH 레지스터로 HSI 8MHz <-> PLL 48MHz 전환, MCO 출력 |
| 05 | [`08_Clock_System/05_SysTick_Reg_c`](Examples/08_Clock_System/05_SysTick_Reg_c) | 레지스터 | (검토 전) SysTick 1ms 틱, `millis()`, 논블로킹 LED/버튼 처리 |
| 06 | [`08_Clock_System/06_NVIC_Priority_c`](Examples/08_Clock_System/06_NVIC_Priority_c) | HAL, NVIC | (검토 전) 선점/서브 우선순위와 인터럽트 중첩 실험, B1로 3가지 케이스 전환 |
| 07 | [`08_Clock_System/07_AFIO_Remap_Reg_c`](Examples/08_Clock_System/07_AFIO_Remap_Reg_c) | 레지스터 | (검토 전) AFIO_MAPR로 JTAG 핀(PB3/PB4/PA15) 해방과 TIM2 핀 리맵 |
| 08 | [`08_Clock_System/08_HardFault_Diagnosis_Reg_c`](Examples/08_Clock_System/08_HardFault_Diagnosis_Reg_c) | 레지스터+어셈블리 | (검토 전) 4종 폴트 유발, 스택 프레임/CFSR/BFAR 해석해 UART 출력 |
| 09 | [`08_Clock_System/09_MCO_ClockOut_c`](Examples/08_Clock_System/09_MCO_ClockOut_c) | HAL, RCC | (검토 전) MCO(PA8)로 HSI/PLL/HSE 클럭 출력, B1로 소스 순환 |
| 10 | [`08_Clock_System/10_DWT_Profiling_Reg_c`](Examples/08_Clock_System/10_DWT_Profiling_Reg_c) | 레지스터 | (검토 전) DWT 사이클 카운터로 연산/함수 실행 시간(사이클) 측정 |
| 11 | [`08_Clock_System/11_NonBlocking_StateMachine_c`](Examples/08_Clock_System/11_NonBlocking_StateMachine_c) | HAL | (검토 전) HAL_Delay 없는 상태 머신 신호등, 버튼 디바운스/이벤트 |
| 12 | [`08_Clock_System/12_DeviceInfo_UniqueID_Reg_c`](Examples/08_Clock_System/12_DeviceInfo_UniqueID_Reg_c) | 레지스터 | (검토 전) 96비트 고유 ID, 플래시 크기, DEV_ID, 리셋 원인 출력 |
| 01 | [`09_WatchDog_Sleep/01_WatchdogTimer`](Examples/09_WatchDog_Sleep/01_WatchdogTimer) | HAL, WWDG | 윈도우 워치독(WWDG) 초기화와 주기적 리프레시 기본 사용 |
| 02 | [`09_WatchDog_Sleep/02_WWDG_c`](Examples/09_WatchDog_Sleep/02_WWDG_c) | HAL, WWDG | (검토 전) 윈도우 워치독(너무 이른/늦은 리프레시 리셋), 조기 경고 콜백 |
| 03 | [`09_WatchDog_Sleep/03_Sleep_Mode_c`](Examples/09_WatchDog_Sleep/03_Sleep_Mode_c) | HAL, PWR | (검토 전) WFI Sleep 모드, 바쁜 대기와 main 루프 횟수 비교 |
| 04 | [`09_WatchDog_Sleep/04_Stop_Mode_EXTI_c`](Examples/09_WatchDog_Sleep/04_Stop_Mode_EXTI_c) | HAL, PWR, EXTI | (검토 전) Stop 모드 진입, 버튼(EXTI) 웨이크업 후 클럭 복구 |
| 05 | [`09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c`](Examples/09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c) | HAL, PWR, RTC | (검토 전) Standby 진입, RTC 알람(10초) 웨이크업, BKP 부팅 카운터 유지 |
| 06 | [`09_WatchDog_Sleep/06_BKP_Tamper_c`](Examples/09_WatchDog_Sleep/06_BKP_Tamper_c) | HAL+레지스터, BKP | (검토 전) 백업 레지스터 유지, TAMPER(PC13) 감지 시 삭제 |
| 07 | [`09_WatchDog_Sleep/07_IWDG_c`](Examples/09_WatchDog_Sleep/07_IWDG_c) | HAL, IWDG | (검토 전) 독립 워치독(LSI), 타임아웃 계산, 리셋 원인 |
| 01 | [`10_Flash_CRC/01_Flash_Write_c`](Examples/10_Flash_CRC/01_Flash_Write_c) | HAL, FLASH | (검토 전) 내부 Flash 페이지에 부팅 횟수 덧붙여 저장(웨어 레벨링) |
| 02 | [`10_Flash_CRC/02_CRC_Unit_c`](Examples/10_Flash_CRC/02_CRC_Unit_c) | HAL, CRC | (검토 전) 하드웨어 CRC-32/MPEG-2와 소프트웨어 구현 비교, 1비트 오류 검출 |
| 01 | [`11_CAN_Communication/01_CAN_Loopback_c`](Examples/11_CAN_Communication/01_CAN_Loopback_c) | HAL, CAN | (검토 전) bxCAN 500kbps 루프백 송수신(트랜시버 불필요), FIFO0 수신 콜백 |
| 02 | [`11_CAN_Communication/02_CAN_Filter_c`](Examples/11_CAN_Communication/02_CAN_Filter_c) | HAL, CAN | (검토 전) bxCAN 수신 필터: ID 리스트/마스크, 표준·확장, FIFO0/1 |
| 01 | [`12_Advanced_Topics/01_ADC_Injected_c`](Examples/12_Advanced_Topics/01_ADC_Injected_c) | HAL, ADC, DMA | (검토 전) 주입 채널이 정규 연속 변환에 끼어들어 온도 센서를 읽음 |
| 02 | [`12_Advanced_Topics/02_ADC_DualMode_c`](Examples/12_Advanced_Topics/02_ADC_DualMode_c) | HAL, ADC, DMA | (검토 전) ADC1+ADC2 동시 샘플링(듀얼 모드), 32비트 DMA |
| 03 | [`12_Advanced_Topics/03_I2C_Slave_Loopback_c`](Examples/12_Advanced_Topics/03_I2C_Slave_Loopback_c) | HAL, I2C | (검토 전) I2C 슬레이브 모드(I2C1 마스터 ↔ I2C2 슬레이브), 리슨/주소 콜백 |
| 04 | [`12_Advanced_Topics/04_SPI_Slave_Loopback_c`](Examples/12_Advanced_Topics/04_SPI_Slave_Loopback_c) | HAL, SPI | (검토 전) SPI 슬레이브 모드(SPI1 마스터 ↔ SPI2 슬레이브), 하드웨어 NSS |
| 05 | [`12_Advanced_Topics/05_TIM_Cascade32_c`](Examples/12_Advanced_Topics/05_TIM_Cascade32_c) | HAL, TIM | (검토 전) 마스터/슬레이브로 잇는 32비트 카운터(71분), 스톱워치 |
| 06 | [`12_Advanced_Topics/06_DMA_Priority_c`](Examples/12_Advanced_Topics/06_DMA_Priority_c) | HAL, DMA | (검토 전) DMA 채널 우선순위/중재, 완료 시각 DWT 측정 |
| 07 | [`12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c`](Examples/12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c) | 레지스터+어셈블리 | (검토 전) SVC/PendSV/PSP 기반 미니 선점형 RTOS(작업 2개 시분할) |
| 08 | [`12_Advanced_Topics/08_HSE_PLL_72MHz_Reg_c`](Examples/12_Advanced_Topics/08_HSE_PLL_72MHz_Reg_c) | 레지스터 | (검토 전) HSE(바이패스)+PLL x9 = 72MHz, 실패 시 HSI 로 복귀 |
| 09 | [`12_Advanced_Topics/09_ITM_SWO_Printf_Reg_c`](Examples/12_Advanced_Topics/09_ITM_SWO_Printf_Reg_c) | 레지스터 | (검토 전) ITM/SWO 로 printf (UART 핀 불필요) |
| 10 | [`12_Advanced_Topics/10_StackWatermark_Reg_c`](Examples/12_Advanced_Topics/10_StackWatermark_Reg_c) | 레지스터 | (검토 전) 스택 페인팅과 하이 워터마크, 재귀 실험 |
| 11 | [`12_Advanced_Topics/11_IAP_Bootloader_Reg_c`](Examples/12_Advanced_Topics/11_IAP_Bootloader_Reg_c) | 레지스터 | (검토 전) IAP 부트로더: 앱 유효성 검사 후 VTOR 교체·점프 (링커 스크립트 포함) |
| 12 | [`12_Advanced_Topics/12_IAP_App_Reg_c`](Examples/12_Advanced_Topics/12_IAP_App_Reg_c) | 레지스터 | (검토 전) 0x08008000 에서 도는 IAP 응용 프로그램 (부트로더와 짝) |
| 13 | [`12_Advanced_Topics/13_IAP_Xmodem_Bootloader_Reg_c`](Examples/12_Advanced_Topics/13_IAP_Xmodem_Bootloader_Reg_c) | 레지스터 | (검토 전) UART XMODEM-CRC 로 펌웨어 수신·Flash 기록·점프하는 부트로더 |
| 14 | [`12_Advanced_Topics/14_OptionBytes_Read_c`](Examples/12_Advanced_Topics/14_OptionBytes_Read_c) | HAL, FLASH | (검토 전) 옵션 바이트 읽기(RDP/WRP/USER), 읽기 전용 |
| 15 | [`12_Advanced_Topics/15_CAN_Normal_2boards_c`](Examples/12_Advanced_Topics/15_CAN_Normal_2boards_c) | HAL, CAN | (검토 전) 실제 CAN 버스(2보드): 오류 카운터, 버스오프 복구 |

예제는 기능별 그룹 폴더 아래에 있고, **예제 폴더 번호는 그룹 안에서 항상 `01`부터 순서대로** 붙인다. 폴더명 끝의 `_c`는 Claude가 작성한 예제(검토 전)를 뜻한다. `12_Advanced_Topics`는 3학년 2학기 기본 진도를 넘어서는 **심화(선택)** 예제만 모아 둔 그룹이며, 나머지 01~11 그룹이 한 학기 분량이다. 그룹별 안내는 [Examples/README.md](Examples/README.md) 참고.

각 예제 폴더는 PlatformIO 프로젝트(`platformio.ini` 포함) 또는 STM32CubeIDE 프로젝트(`.project`/`.cproject`/`.ioc` 포함) 형태이며, VS Code에서 PlatformIO 확장으로 열어 빌드·업로드하는 것을 기본으로 한다.

## 참고 문서 (`Docs/`)

각 문서의 용도, 처음 읽는 순서, RM0008/PM0056 장 지도, 보드 클럭 구성은 [Docs/README.md](Docs/README.md) 에 정리되어 있다.

| 문서 | 설명 |
|---|---|
| [UM1724](Docs/UM1724_User_Manual_STM32Nucleo64.pdf) | Nucleo-64 보드 사용자 매뉴얼 |
| [DS5319 (데이터시트)](Docs/Datasheet_stm32f103.pdf) | STM32F103x8/xB 데이터시트 |
| [RM0008](Docs/RM0008_Reference_manual.pdf) | STM32F101/102/103/105/107 레지스터 레퍼런스 매뉴얼 |
| [PM0056](Docs/PM0056_CortexM3_Programming_Manual.pdf) | Cortex-M3 프로그래밍 매뉴얼 |
| [UM1850](Docs/HAL_UM1850_hal_and_lowlayer_drivers.pdf) | STM32F1 HAL/LL 드라이버 사용자 매뉴얼 |
| [MB1136 회로도](Docs/Schematic_STM32F103RB.pdf) | Nucleo-F103RB 보드 회로도 |
| [UM1718 (st.com)](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx.pdf) | STM32CubeMX 사용자 매뉴얼 (77MB라 저장소에 넣지 않고 ST 사이트로 연결) |
| [PM0075](Docs/PM0075-stm32f10xxx-flash-memory.pdf) | STM32F10xxx Flash 메모리 프로그래밍 매뉴얼(쓰기·지우기·옵션 바이트) |
| [ES096](Docs/ES096_F103x8xB_Errata.pdf) | STM32F103x8/xB(중밀도, F103RB) 디바이스 errata(오류 정정표) |
| [AN2834](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2834_ADC_Accuracy.pdf) | ADC 정확도 최적화 |
| [AN4013](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4013_Timers_Introduction.pdf) | STM32 타이머 개요 |
| [AN4776](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4776_General_Purpose_Timer.pdf) | 범용 타이머 활용 |
| [AN2548](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2548_DMA_Controller.pdf) | DMA 컨트롤러 사용법 |
| [AN2586](Docs/AN2586_Hardware_Development_Getting_Started.pdf) | STM32F10xxx 하드웨어 개발 시작 가이드 |
| [AN2606](Docs/AN2606_System_Memory_Boot_Mode.pdf) | STM32 시스템 메모리 부트 모드(부트로더) |
| [DB2196](Docs/DB2196_Nucleo_64_boards.pdf) | Nucleo-64 보드 데이터브리프 |
| [UM1727](Docs/UM1727_Getting_started.pdf) | Nucleo 보드 시작하기 가이드(2016, 오래된 도구 기준) |
| [NUCLEO-F103RB 제품 페이지 (st.com)](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196) | ST 공식 제품 페이지 (`Docs/NUCLEO-F103RB.url` 바로가기와 같은 주소) |

## 회로 시뮬레이션
Proteus 프로젝트는 각 예제 폴더에 있다. 파일 이름은 `STM32F103R_ArduinoPinmap.pdsprj`이다(`01_GPIO/07_ReadPinReg`만 `STM32F103R.pdsprj`). Proteus에서 열 때 MCU의 펌웨어 경로(`firmware.elf`)를 해당 예제의 빌드 결과로 지정한다.

### Proteus의 MCU 모델: F103RB 대신 F103C8

Proteus(8.17 기준) 라이브러리에는 **STM32F103RB 모델이 없다**. 제공되는 STM32F103 부품은 C4, C6, C8, R4, R6, T4, T6뿐이다. 그래서 프로젝트의 MCU는 그중 F103RB와 같은 medium-density 칩인 **STM32F103C8**(48핀)을 쓴다. 파일 이름에 `R`이 들어 있지만 실제 MCU는 C8이다.

| 항목 | STM32F103C8 (Proteus) | STM32F103RB (Nucleo) |
|---|---|---|
| 계열 | medium-density | medium-density |
| SRAM | 20 KB | 20 KB |
| 주변장치 | TIM1~4, SPI1~2, I2C1~2, USART1~3, USB, CAN | 같음 |
| Flash | **64 KB** | 128 KB |
| 패키지 / GPIO | **48핀 / 37개** | 64핀 / 51개 |

같은 계열이라 레지스터 주소, 주변장치 구성, 인터럽트 벡터, DMA 채널 배정은 같다(DS5319 Table 2). 차이는 Flash 크기와 핀 수이다.

- **포트 C가 거의 없다**: 48핀 패키지에는 **PC0~PC12와 PD2가 없고, 포트 C는 PC13~PC15만 있다**. Nucleo Arduino 헤더 가운데 포트 C에 연결된 핀(D9 = PC7, A4 = PC1, A5 = PC0 등)을 쓰는 예제는 Proteus에서 그 핀을 시뮬레이션할 수 없다(예: `01_GPIO/10_AD2Test`). PA·PB 핀과 PC13(사용자 버튼 B1)만 쓰는 예제는 문제없다.
- **Flash 64 KB**: 펌웨어가 64 KB를 넘으면 C8 모델에 올라가지 않는다. 현재 예제들은 대부분 이보다 훨씬 작다.
- 64핀 R6 모델은 핀은 맞지만 low-density 칩이다. Flash가 작고 TIM4·USART3·I2C2·SPI2 같은 medium-density 주변장치가 없어 RB용 펌웨어와 맞지 않는다.

Proteus의 STM32 모델 파일에는 DMA, RTC, IWDG, WWDG, ADC, USB 등의 모델이 들어 있다. 반면 **저전력 모드(PWR의 Sleep/Stop/Standby), 백업 레지스터·탬퍼(BKP), CAN은 모델링되어 있지 않은 것으로 보인다**. 이 판단은 설치된 모델 파일을 분석한 결과이다(Labcenter의 공식 목록은 없다). `09_WatchDog_Sleep`의 저전력·BKP 예제와 `11_CAN_Communication`은 실제 보드로 확인한다.

### Wokwi

`01_GPIO/02_BlinkHAL`에는 [Wokwi](https://wokwi.com) 시뮬레이터 설정(`wokwi.toml`, `diagram.json`)도 있다. Wokwi 역시 F103RB가 없어 Blue Pill(STM32F103C8)을 쓰며, DMA·RTC·PWR·IWDG는 지원하지 않는다. 사용법은 그 예제의 README를 참고한다.

## 관련 교과목
- 디지털논리회로, 디지털시스템, 마이크로컨트롤러
- 프로그래밍 언어 (C or C++)
- 회로이론, 전자회로 

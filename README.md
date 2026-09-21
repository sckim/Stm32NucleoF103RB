# Stm32NucleoF103RB

**임베디드시스템** 교과목의 실습 자료 저장소이다. STM32 Nucleo-F103RB(ARM Cortex-M3) 보드를 기준으로 GPIO부터 인터럽트, 타이머, ADC, UART, I2C까지 단계별 예제 프로젝트와 참고 문서, 회로 시뮬레이션 파일을 정리한다.

## 개발 환경

* IDE: [VS Code](https://code.visualstudio.com/) + [PlatformIO](https://platformio.org/) (예제 대부분), 일부는 STM32CubeIDE 프로젝트 파일(`.project`, `.cproject`, `.ioc`)을 함께 포함함
* 보드 설정: [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)로 생성한 `.ioc` 파일 기준
* 드라이버: STM32 HAL 및 LL(Low-Layer) 라이브러리, 일부 예제는 레지스터 직접 제어 방식
* 회로 시뮬레이션: [Proteus](https://www.labcenter.com/) (`STM32F103R.pdsprj`)

## 폴더 구조

| 폴더/파일 | 내용 |
|---|---|
| `Docs/` | 데이터시트, 레퍼런스 매뉴얼 등 STM32F103 관련 공식 문서(PDF) |
| `Examples/` | 실습 예제 프로젝트 모음 |
| `Simulation/` | Proteus 회로 시뮬레이션 프로젝트 |

## 실습 예제 (`Examples/`)

| 번호 | 예제 (`그룹/예제`) | 구현 방식 | 내용 |
|---|---|---|---|
| 00 | `01_GPIO/00_BlinkArduino` | Arduino 프레임워크(PlatformIO) | `pinMode`/`digitalWrite`로 온보드 LED(PA5) 점멸 |
| 01 | `01_GPIO/01_BlinkHAL` | HAL 드라이버 | HAL API 기반 LED 점멸 |
| 02 | `01_GPIO/02_BlinkLL` | LL 드라이버 | LL API 기반 LED 점멸 |
| 03 | `01_GPIO/03_BlinkReg` | 레지스터 직접 제어 | `RCC`, `GPIOx->CRL` 레지스터를 직접 조작하여 LED 점멸, SysTick 기반 딜레이 구현 |
| 04 | `01_GPIO/04_ReadPin` | HAL | 사용자 버튼(B1) 폴링 입력으로 LED(LD2) 제어 |
| 05 | `01_GPIO/05_ExtInt` | HAL, EXTI | 버튼 입력을 EXTI 인터럽트로 처리(`HAL_GPIO_EXTI_Callback`) |
| 06 | `01_GPIO/06_ReadPinReg` | 레지스터 | `GPIOC->IDR` 직접 읽기로 버튼 폴링 (04_ReadPin의 레지스터 버전) |
| 07 | `01_GPIO/07_ExternalInt` | HAL, EXTI | 두 개 EXTI 라인(`EXTI9_5`, `EXTI15_10`)의 버튼 B1(PC13)·B2(PA6)를 하나의 `HAL_GPIO_EXTI_Callback`에서 구분 처리 |
| 08 | `01_GPIO/08_BitBanding_Reg_c` | 레지스터 | (검토 전) Cortex-M3 비트 밴딩 별칭으로 RCC/GPIO/SRAM 비트를 원자적으로 접근 |
| 10 | `02_USART/10_USART_printf` | HAL, UART | `printf` 출력을 USART로 리다이렉션 |
| 11 | `02_USART/11_USART_Interrupt_RX_HAL_c` | HAL, UART, NVIC | (검토 전) USART2 수신 인터럽트(`HAL_UART_RxCpltCallback`)로 에코 및 LED 제어 |
| 12 | `02_USART/12_USART_DMA_IdleLine_HAL_c` | HAL, UART, DMA | (검토 전) DMA 원형 수신 + IDLE 라인 감지로 가변 길이 패킷 처리 |
| 13 | `02_USART/13_USART_RingBuffer_HAL_c` | HAL, UART | (검토 전) ISR->main 링 버퍼(lock-free), 오버플로 시험(B1로 main 정지) |
| 14 | `02_USART/14_UART_CLI_HAL_c` | HAL, UART | (검토 전) 명령줄 인터페이스: 줄 편집, 명령 테이블(help/led/uptime/echo/peek/reset) |
| 20 | `03_ADC/20_ADC_Temperature` | HAL, ADC | 내부 온도 센서 ADC 값을 UART로 출력 |
| 21 | `03_ADC/21_ADC_DMA_TimTrigger_HAL_c` | HAL, ADC, DMA, TIM | (검토 전) TIM3 TRGO 트리거 1kHz 2채널 스캔, DMA 원형 저장, HT/TC 콜백 |
| 22 | `03_ADC/22_ADC_AnalogWatchdog_HAL_c` | HAL, ADC, DMA | (검토 전) ADC 아날로그 워치독으로 전압 창(window) 이탈 감지 인터럽트 |
| 23 | `03_ADC/23_ADC_Vrefint_VDDA_HAL_c` | HAL, ADC | (검토 전) Vrefint 로 실제 VDDA 를 측정해 ADC 값을 보정 |
| 30 | `04_I2C_Communication/30_PCF8574` | HAL, I2C | PCF8574 I2C 확장 IC를 통한 문자 LCD 제어, I2C 주소 스캐너 포함 |
| 31 | `04_I2C_Communication/31_I2C_Scan_HAL_c` | HAL, I2C | (검토 전) I2C1 버스 스캐너(0x08~0x77), 흔한 장치 이름 표시 |
| 32 | `04_I2C_Communication/32_I2C_MPU6050_HAL_c` | HAL, I2C | (검토 전) MPU6050 WHO_AM_I 확인과 가속도/자이로/온도 14바이트 연속 읽기 |
| 40 | `05_SPI_Communication/40_SPI_Loopback_HAL_c` | HAL, SPI | (검토 전) SPI1 마스터 루프백(MOSI-MISO 점퍼), 분주비별 전송 속도 측정 |
| 41 | `05_SPI_Communication/41_SPI_DMA_Fullduplex_HAL_c` | HAL, SPI, DMA | (검토 전) SPI1 전이중 DMA 전송, 폴링 방식과 시간/CPU 여유 비교 |
| 50 | `06_Timers_RTC/50_TIM_TimeBase` | HAL, TIM | 타이머 주기 인터럽트(`HAL_TIM_PeriodElapsedCallback`)로 LED 토글 |
| 51 | `06_Timers_RTC/51_TIM_PWM_HAL_c` | HAL, TIM | (검토 전) TIM2_CH1(PA0) 1kHz PWM, LED 밝기 왕복 |
| 52 | `06_Timers_RTC/52_TIM_InputCapture_HAL_c` | HAL, TIM | (검토 전) 입력 캡처로 주기/주파수 측정 (TIM3 PWM 테스트 신호 포함) |
| 53 | `06_Timers_RTC/53_TIM1_DeadTime_Break_HAL_c` | HAL, TIM1 | (검토 전) 상보 PWM + 데드타임(약 1us) + 브레이크 입력/소프트웨어 브레이크 |
| 54 | `06_Timers_RTC/54_RTC_Calendar_HAL_c` | HAL+레지스터, RTC | (검토 전) F1 RTC(32비트 카운터)를 Unix 시간으로 쓰는 달력 시계, 1초 인터럽트 |
| 55 | `06_Timers_RTC/55_TIM_PWMInput_HAL_c` | HAL, TIM | (검토 전) PWM 입력 모드(슬레이브 리셋)로 주파수/듀티 하드웨어 측정 |
| 56 | `06_Timers_RTC/56_TIM_Encoder_HAL_c` | HAL, TIM | (검토 전) 엔코더 인터페이스 모드(x4), 소프트웨어 직교 신호 시뮬레이션 포함 |
| 57 | `06_Timers_RTC/57_TIM_OnePulse_HCSR04_HAL_c` | HAL, TIM | (검토 전) 원 펄스 모드 10us 트리거 + 입력 캡처로 HC-SR04 거리 측정 |
| 60 | `07_DMA/60_DMA_MemToMem_HAL_c` | HAL, DMA | (검토 전) 메모리->메모리 DMA 복사, CPU 복사와 DWT 사이클 비교 |
| 70 | `08_Clock_System/70_Clock_Config_Reg_c` | 레지스터 | (검토 전) RCC/FLASH 레지스터로 HSI 8MHz <-> PLL 48MHz 전환, MCO 출력 |
| 71 | `08_Clock_System/71_SysTick_Reg_c` | 레지스터 | (검토 전) SysTick 1ms 틱, `millis()`, 논블로킹 LED/버튼 처리 |
| 72 | `08_Clock_System/72_NVIC_Priority_HAL_c` | HAL, NVIC | (검토 전) 선점/서브 우선순위와 인터럽트 중첩 실험, B1로 3가지 케이스 전환 |
| 73 | `08_Clock_System/73_AFIO_Remap_Reg_c` | 레지스터 | (검토 전) AFIO_MAPR로 JTAG 핀(PB3/PB4/PA15) 해방과 TIM2 핀 리맵 |
| 74 | `08_Clock_System/74_HardFault_Diagnosis_Reg_c` | 레지스터+어셈블리 | (검토 전) 4종 폴트 유발, 스택 프레임/CFSR/BFAR 해석해 UART 출력 |
| 75 | `08_Clock_System/75_PendSV_ContextSwitch_Reg_c` | 레지스터+어셈블리 | (검토 전) SVC/PendSV/PSP 기반 미니 선점형 RTOS(작업 2개 시분할) |
| 76 | `08_Clock_System/76_MCO_ClockOut_HAL_c` | HAL, RCC | (검토 전) MCO(PA8)로 HSI/PLL/HSE 클럭 출력, B1로 소스 순환 |
| 77 | `08_Clock_System/77_DWT_Profiling_Reg_c` | 레지스터 | (검토 전) DWT 사이클 카운터로 연산/함수 실행 시간(사이클) 측정 |
| 78 | `08_Clock_System/78_NonBlocking_StateMachine_HAL_c` | HAL | (검토 전) HAL_Delay 없는 상태 머신 신호등, 버튼 디바운스/이벤트 |
| 79 | `08_Clock_System/79_DeviceInfo_UniqueID_Reg_c` | 레지스터 | (검토 전) 96비트 고유 ID, 플래시 크기, DEV_ID, 리셋 원인 출력 |
| 80 | `09_WatchDog_Sleep/80_WatchdogTimer` | HAL, WWDG | 윈도우 워치독(WWDG) 초기화와 주기적 리프레시 기본 사용 |
| 81 | `09_WatchDog_Sleep/81_WWDG_HAL_c` | HAL, WWDG | (검토 전) 윈도우 워치독(너무 이른/늦은 리프레시 리셋), 조기 경고 콜백 |
| 82 | `09_WatchDog_Sleep/82_Sleep_Mode_HAL_c` | HAL, PWR | (검토 전) WFI Sleep 모드, 바쁜 대기와 main 루프 횟수 비교 |
| 83 | `09_WatchDog_Sleep/83_Stop_Mode_EXTI_HAL_c` | HAL, PWR, EXTI | (검토 전) Stop 모드 진입, 버튼(EXTI) 웨이크업 후 클럭 복구 |
| 84 | `09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c` | HAL, PWR, RTC | (검토 전) Standby 진입, RTC 알람(10초) 웨이크업, BKP 부팅 카운터 유지 |
| 90 | `10_Flash_CRC/90_Flash_Write_HAL_c` | HAL, FLASH | (검토 전) 내부 Flash 페이지에 부팅 횟수 덧붙여 저장(웨어 레벨링) |
| 91 | `10_Flash_CRC/91_CRC_Unit_HAL_c` | HAL, CRC | (검토 전) 하드웨어 CRC-32/MPEG-2와 소프트웨어 구현 비교, 1비트 오류 검출 |
| 100 | `11_CAN_Communication/100_CAN_Loopback_HAL_c` | HAL, CAN | (검토 전) bxCAN 500kbps 루프백 송수신(트랜시버 불필요), FIFO0 수신 콜백 |

예제는 기능별 그룹 폴더(`01_GPIO`, `02_USART`, `03_ADC`, `04_I2C_Communication`, `05_SPI_Communication`, `06_Timers_RTC`, `07_DMA`, `08_Clock_System`, `09_WatchDog_Sleep`, `10_Flash_CRC`, `11_CAN_Communication`) 아래에 있으며, 예제 번호는 그룹과 무관하게 유지된다. 폴더명 끝의 `_c`는 Claude가 작성한 예제(검토 전)를 뜻한다.

각 예제 폴더는 PlatformIO 프로젝트(`platformio.ini` 포함) 또는 STM32CubeIDE 프로젝트(`.project`/`.cproject`/`.ioc` 포함) 형태이며, VS Code에서 PlatformIO 확장으로 열어 빌드·업로드하는 것을 기본으로 한다.

## 참고 문서 (`Docs/`)

각 문서의 개정·쪽수, 용도, RM0008/PM0056 장 지도, 보드 클럭 구성은 [Docs/README.md](Docs/README.md) 에 정리되어 있다.

| 문서 | 설명 |
|---|---|
| [RM0008_Reference_manual.pdf](Docs/RM0008_Reference_manual.pdf) | STM32F101/102/103/105/107 레지스터 레퍼런스 매뉴얼 |
| [HAL_UM1850_hal_and_lowlayer_drivers.pdf](Docs/HAL_UM1850_hal_and_lowlayer_drivers.pdf) | STM32F1 HAL/LL 드라이버 사용자 매뉴얼 |
| [PM0056-...cortexm3-programming-manual...pdf](<Docs/PM0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf>) | Cortex-M3 프로그래밍 매뉴얼 |
| [UM1724_User_Manual_STM32Nucleo64.pdf](Docs/UM1724_User_Manual_STM32Nucleo64.pdf) | Nucleo-64 보드 사용자 매뉴얼 |
| [UM1727_Getting_started.pdf](Docs/UM1727_Getting_started.pdf) | Nucleo 보드 시작하기 가이드 |
| [DB2196_Nucleo_64_boards.pdf](Docs/DB2196_Nucleo_64_boards.pdf) | Nucleo-64 보드 데이터브리프 |
| [Datasheet_stm32f103.pdf](Docs/Datasheet_stm32f103.pdf) | STM32F103 데이터시트 |
| [Schematic_STM32F103RB.pdf](Docs/Schematic_STM32F103RB.pdf) | Nucleo-F103RB 회로도 |
| [AN2586-...hardware-development...pdf](<Docs/AN2586-getting-started-with-stm32f10xxx-hardware-development-stmicroelectronics.pdf>) | STM32F10xxx 하드웨어 개발 시작 가이드 |
| [AN2606-...system-memory-boot-mode...pdf](<Docs/AN2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf>) | STM32 시스템 메모리 부트 모드 안내 |
| [ES0340-...device-errata...pdf](<Docs/ES0340-stm32f101xcde-stm32f103xcde-device-errata-stmicroelectronics.pdf>) | STM32F101/103 디바이스 errata(오류 정정표) |
| [NUCLEO-F103RB.url](Docs/NUCLEO-F103RB.url) | [ST 공식 제품 페이지](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196) 바로가기 |

## 회로 시뮬레이션
`STM32F103R.pdsprj`는 Proteus용 회로 시뮬레이션 프로젝트

## 관련 교과목
- 디지털논리회로, 디지털시스템, 마이크로컨트롤러
- 프로그래밍 언어 (C or C++)
- 회로이론, 전자회로 

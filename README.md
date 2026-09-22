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
| 01 | `01_GPIO/01_BlinkArduino` | Arduino 프레임워크(PlatformIO) | `pinMode`/`digitalWrite`로 온보드 LED(PA5) 점멸 |
| 02 | `01_GPIO/02_BlinkHAL` | HAL 드라이버 | HAL API 기반 LED 점멸 |
| 03 | `01_GPIO/03_BlinkLL` | LL 드라이버 | LL API 기반 LED 점멸 |
| 04 | `01_GPIO/04_BlinkReg` | 레지스터 직접 제어 | `RCC`, `GPIOx->CRL` 레지스터를 직접 조작하여 LED 점멸, SysTick 기반 딜레이 구현 |
| 05 | `01_GPIO/05_ReadPin` | HAL | 사용자 버튼(B1) 폴링 입력으로 LED(LD2) 제어 |
| 06 | `01_GPIO/06_ExtInt` | HAL, EXTI | 버튼 입력을 EXTI 인터럽트로 처리(`HAL_GPIO_EXTI_Callback`) |
| 07 | `01_GPIO/07_ReadPinReg` | 레지스터 | `GPIOC->IDR` 직접 읽기로 버튼 폴링 (05_ReadPin의 레지스터 버전) |
| 08 | `01_GPIO/08_ExternalInt` | HAL, EXTI | 두 개 EXTI 라인(`EXTI9_5`, `EXTI15_10`)의 버튼 B1(PC13)·B2(PA6)를 하나의 `HAL_GPIO_EXTI_Callback`에서 구분 처리 |
| 09 | `01_GPIO/09_BitBanding_Reg_c` | 레지스터 | (검토 전) Cortex-M3 비트 밴딩 별칭으로 RCC/GPIO/SRAM 비트를 원자적으로 접근 |
| 01 | `02_USART/01_USART_printf` | HAL, UART | `printf` 출력을 USART로 리다이렉션 |
| 02 | `02_USART/02_USART_Interrupt_RX_HAL_c` | HAL, UART, NVIC | (검토 전) USART2 수신 인터럽트(`HAL_UART_RxCpltCallback`)로 에코 및 LED 제어 |
| 03 | `02_USART/03_USART_DMA_IdleLine_HAL_c` | HAL, UART, DMA | (검토 전) DMA 원형 수신 + IDLE 라인 감지로 가변 길이 패킷 처리 |
| 04 | `02_USART/04_USART_RingBuffer_HAL_c` | HAL, UART | (검토 전) ISR->main 링 버퍼(lock-free), 오버플로 시험(B1로 main 정지) |
| 05 | `02_USART/05_UART_CLI_HAL_c` | HAL, UART | (검토 전) 명령줄 인터페이스: 줄 편집, 명령 테이블(help/led/uptime/echo/peek/reset) |
| 06 | `02_USART/06_USART_HalfDuplex_HAL_c` | HAL, UART | (검토 전) USART 단선 반이중(PA9 한 선), 방향 전환, 오픈드레인 버스 |
| 01 | `03_ADC/01_ADC_Temperature` | HAL, ADC | 내부 온도 센서 ADC 값을 UART로 출력 |
| 02 | `03_ADC/02_ADC_DMA_TimTrigger_HAL_c` | HAL, ADC, DMA, TIM | (검토 전) TIM3 TRGO 트리거 1kHz 2채널 스캔, DMA 원형 저장, HT/TC 콜백 |
| 03 | `03_ADC/03_ADC_AnalogWatchdog_HAL_c` | HAL, ADC, DMA | (검토 전) ADC 아날로그 워치독으로 전압 창(window) 이탈 감지 인터럽트 |
| 04 | `03_ADC/04_ADC_Vrefint_VDDA_HAL_c` | HAL, ADC | (검토 전) Vrefint 로 실제 VDDA 를 측정해 ADC 값을 보정 |
| 05 | `03_ADC/05_ADC_Potentiometer_PWM_HAL_c` | HAL, ADC, TIM | (검토 전) 가변저항 → 이동평균/히스테리시스/감마 → PWM 밝기 |
| 01 | `04_I2C_Communication/01_PCF8574` | HAL, I2C | PCF8574 I2C 확장 IC를 통한 문자 LCD 제어, I2C 주소 스캐너 포함 |
| 02 | `04_I2C_Communication/02_I2C_Scan_HAL_c` | HAL, I2C | (검토 전) I2C1 버스 스캐너(0x08~0x77), 흔한 장치 이름 표시 |
| 03 | `04_I2C_Communication/03_I2C_MPU6050_HAL_c` | HAL, I2C | (검토 전) MPU6050 WHO_AM_I 확인과 가속도/자이로/온도 14바이트 연속 읽기 |
| 04 | `04_I2C_Communication/04_I2C_MPU6050_DMA_HAL_c` | HAL, I2C, DMA | (검토 전) 논블로킹 I2C(DMA)로 MPU6050 읽기, 오류 복구 |
| 05 | `04_I2C_Communication/05_I2C_OLED_SSD1306_HAL_c` | HAL, I2C | (검토 전) SSD1306 OLED, 프레임버퍼, 선/숫자/애니메이션 |
| 06 | `04_I2C_Communication/06_I2C_EEPROM_AT24C_HAL_c` | HAL, I2C | (검토 전) AT24Cxx EEPROM: 16비트 주소, 페이지 경계, ACK 폴링 |
| 07 | `04_I2C_Communication/07_I2C_DS3231_RTC_HAL_c` | HAL, I2C | (검토 전) 외장 RTC DS3231: BCD 시각, 온도, OSF 플래그 |
| 01 | `05_SPI_Communication/01_SPI_Loopback_HAL_c` | HAL, SPI | (검토 전) SPI1 마스터 루프백(MOSI-MISO 점퍼), 분주비별 전송 속도 측정 |
| 02 | `05_SPI_Communication/02_SPI_DMA_Fullduplex_HAL_c` | HAL, SPI, DMA | (검토 전) SPI1 전이중 DMA 전송, 폴링 방식과 시간/CPU 여유 비교 |
| 03 | `05_SPI_Communication/03_SPI_OLED_SSD1306_HAL_c` | HAL, SPI | (검토 전) SPI 로 SSD1306 OLED (D/C 핀, 8MHz) |
| 04 | `05_SPI_Communication/04_SPI_SDcard_HAL_c` | HAL, SPI | (검토 전) SPI 모드 SD 카드 초기화, 섹터 읽기(MBR), 용량 계산 |
| 05 | `05_SPI_Communication/05_SPI_Flash_W25Qxx_HAL_c` | HAL, SPI | (검토 전) SPI NOR Flash(W25Qxx): JEDEC ID, 섹터 소거, 페이지 프로그램 |
| 06 | `05_SPI_Communication/06_SPI_MAX7219_HAL_c` | HAL, SPI | (검토 전) MAX7219 8x8 LED 매트릭스, 16비트 프레임/LOAD 반영 |
| 01 | `06_Timers_RTC/01_TIM_TimeBase` | HAL, TIM | 타이머 주기 인터럽트(`HAL_TIM_PeriodElapsedCallback`)로 LED 토글 |
| 02 | `06_Timers_RTC/02_TIM_PWM_HAL_c` | HAL, TIM | (검토 전) TIM2_CH1(PA0) 1kHz PWM, LED 밝기 왕복 |
| 03 | `06_Timers_RTC/03_TIM_InputCapture_HAL_c` | HAL, TIM | (검토 전) 입력 캡처로 주기/주파수 측정 (TIM3 PWM 테스트 신호 포함) |
| 04 | `06_Timers_RTC/04_TIM1_DeadTime_Break_HAL_c` | HAL, TIM1 | (검토 전) 상보 PWM + 데드타임(약 1us) + 브레이크 입력/소프트웨어 브레이크 |
| 05 | `06_Timers_RTC/05_RTC_Calendar_HAL_c` | HAL+레지스터, RTC | (검토 전) F1 RTC(32비트 카운터)를 Unix 시간으로 쓰는 달력 시계, 1초 인터럽트 |
| 06 | `06_Timers_RTC/06_TIM_PWMInput_HAL_c` | HAL, TIM | (검토 전) PWM 입력 모드(슬레이브 리셋)로 주파수/듀티 하드웨어 측정 |
| 07 | `06_Timers_RTC/07_TIM_Encoder_HAL_c` | HAL, TIM | (검토 전) 엔코더 인터페이스 모드(x4), 소프트웨어 직교 신호 시뮬레이션 포함 |
| 08 | `06_Timers_RTC/08_TIM_OnePulse_HCSR04_HAL_c` | HAL, TIM | (검토 전) 원 펄스 모드 10us 트리거 + 입력 캡처로 HC-SR04 거리 측정 |
| 09 | `06_Timers_RTC/09_TIM_Servo_HAL_c` | HAL, TIM | (검토 전) RC 서보: 50Hz PWM, 1~2ms = 0~180° |
| 10 | `06_Timers_RTC/10_TIM_MotorDriver_TB6612_HAL_c` | HAL, TIM | (검토 전) H-브리지 모터: 20kHz PWM, 쇼트 브레이크/코스트, 비상 정지 |
| 01 | `07_DMA/01_DMA_MemToMem_HAL_c` | HAL, DMA | (검토 전) 메모리->메모리 DMA 복사, CPU 복사와 DWT 사이클 비교 |
| 02 | `07_DMA/02_DMA_Circular_GPIO_HAL_c` | HAL, DMA, TIM | (검토 전) 원형 DMA + 핑퐁 버퍼로 GPIOC 패턴 자동 출력 |
| 01 | `08_Clock_System/01_CoreRegisters_Instructions_Reg_c` | 레지스터+ASM | (검토 전) 레지스터 파일, 조건 플래그, IT 조건부실행, 스택 PUSH/POP, 하드웨어 나눗셈 |
| 02 | `08_Clock_System/02_MemoryMap_Reg_c` | 레지스터 | (검토 전) 4GB 주소 공간, 버스 계층, memory-mapped I/O, 리틀 엔디안 |
| 03 | `08_Clock_System/03_ExceptionModel_VectorTable_Reg_c` | 레지스터+ASM | (검토 전) 벡터 테이블, 예외 자동 스택 저장, EXC_RETURN, SVC |
| 04 | `08_Clock_System/04_Clock_Config_Reg_c` | 레지스터 | (검토 전) RCC/FLASH 레지스터로 HSI 8MHz <-> PLL 48MHz 전환, MCO 출력 |
| 05 | `08_Clock_System/05_SysTick_Reg_c` | 레지스터 | (검토 전) SysTick 1ms 틱, `millis()`, 논블로킹 LED/버튼 처리 |
| 06 | `08_Clock_System/06_NVIC_Priority_HAL_c` | HAL, NVIC | (검토 전) 선점/서브 우선순위와 인터럽트 중첩 실험, B1로 3가지 케이스 전환 |
| 07 | `08_Clock_System/07_AFIO_Remap_Reg_c` | 레지스터 | (검토 전) AFIO_MAPR로 JTAG 핀(PB3/PB4/PA15) 해방과 TIM2 핀 리맵 |
| 08 | `08_Clock_System/08_HardFault_Diagnosis_Reg_c` | 레지스터+어셈블리 | (검토 전) 4종 폴트 유발, 스택 프레임/CFSR/BFAR 해석해 UART 출력 |
| 09 | `08_Clock_System/09_MCO_ClockOut_HAL_c` | HAL, RCC | (검토 전) MCO(PA8)로 HSI/PLL/HSE 클럭 출력, B1로 소스 순환 |
| 10 | `08_Clock_System/10_DWT_Profiling_Reg_c` | 레지스터 | (검토 전) DWT 사이클 카운터로 연산/함수 실행 시간(사이클) 측정 |
| 11 | `08_Clock_System/11_NonBlocking_StateMachine_HAL_c` | HAL | (검토 전) HAL_Delay 없는 상태 머신 신호등, 버튼 디바운스/이벤트 |
| 12 | `08_Clock_System/12_DeviceInfo_UniqueID_Reg_c` | 레지스터 | (검토 전) 96비트 고유 ID, 플래시 크기, DEV_ID, 리셋 원인 출력 |
| 01 | `09_WatchDog_Sleep/01_WatchdogTimer` | HAL, WWDG | 윈도우 워치독(WWDG) 초기화와 주기적 리프레시 기본 사용 |
| 02 | `09_WatchDog_Sleep/02_WWDG_HAL_c` | HAL, WWDG | (검토 전) 윈도우 워치독(너무 이른/늦은 리프레시 리셋), 조기 경고 콜백 |
| 03 | `09_WatchDog_Sleep/03_Sleep_Mode_HAL_c` | HAL, PWR | (검토 전) WFI Sleep 모드, 바쁜 대기와 main 루프 횟수 비교 |
| 04 | `09_WatchDog_Sleep/04_Stop_Mode_EXTI_HAL_c` | HAL, PWR, EXTI | (검토 전) Stop 모드 진입, 버튼(EXTI) 웨이크업 후 클럭 복구 |
| 05 | `09_WatchDog_Sleep/05_Standby_RTC_Wakeup_HAL_c` | HAL, PWR, RTC | (검토 전) Standby 진입, RTC 알람(10초) 웨이크업, BKP 부팅 카운터 유지 |
| 06 | `09_WatchDog_Sleep/06_BKP_Tamper_HAL_c` | HAL+레지스터, BKP | (검토 전) 백업 레지스터 유지, TAMPER(PC13) 감지 시 삭제 |
| 07 | `09_WatchDog_Sleep/07_IWDG_HAL_c` | HAL, IWDG | (검토 전) 독립 워치독(LSI), 타임아웃 계산, 리셋 원인 |
| 01 | `10_Flash_CRC/01_Flash_Write_HAL_c` | HAL, FLASH | (검토 전) 내부 Flash 페이지에 부팅 횟수 덧붙여 저장(웨어 레벨링) |
| 02 | `10_Flash_CRC/02_CRC_Unit_HAL_c` | HAL, CRC | (검토 전) 하드웨어 CRC-32/MPEG-2와 소프트웨어 구현 비교, 1비트 오류 검출 |
| 01 | `11_CAN_Communication/01_CAN_Loopback_HAL_c` | HAL, CAN | (검토 전) bxCAN 500kbps 루프백 송수신(트랜시버 불필요), FIFO0 수신 콜백 |
| 02 | `11_CAN_Communication/02_CAN_Filter_HAL_c` | HAL, CAN | (검토 전) bxCAN 수신 필터: ID 리스트/마스크, 표준·확장, FIFO0/1 |
| 01 | `12_Advanced_Topics/01_ADC_Injected_HAL_c` | HAL, ADC, DMA | (검토 전) 주입 채널이 정규 연속 변환에 끼어들어 온도 센서를 읽음 |
| 02 | `12_Advanced_Topics/02_ADC_DualMode_HAL_c` | HAL, ADC, DMA | (검토 전) ADC1+ADC2 동시 샘플링(듀얼 모드), 32비트 DMA |
| 03 | `12_Advanced_Topics/03_I2C_Slave_Loopback_HAL_c` | HAL, I2C | (검토 전) I2C 슬레이브 모드(I2C1 마스터 ↔ I2C2 슬레이브), 리슨/주소 콜백 |
| 04 | `12_Advanced_Topics/04_SPI_Slave_Loopback_HAL_c` | HAL, SPI | (검토 전) SPI 슬레이브 모드(SPI1 마스터 ↔ SPI2 슬레이브), 하드웨어 NSS |
| 05 | `12_Advanced_Topics/05_TIM_Cascade32_HAL_c` | HAL, TIM | (검토 전) 마스터/슬레이브로 잇는 32비트 카운터(71분), 스톱워치 |
| 06 | `12_Advanced_Topics/06_DMA_Priority_HAL_c` | HAL, DMA | (검토 전) DMA 채널 우선순위/중재, 완료 시각 DWT 측정 |
| 07 | `12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c` | 레지스터+어셈블리 | (검토 전) SVC/PendSV/PSP 기반 미니 선점형 RTOS(작업 2개 시분할) |
| 08 | `12_Advanced_Topics/08_HSE_PLL_72MHz_Reg_c` | 레지스터 | (검토 전) HSE(바이패스)+PLL x9 = 72MHz, 실패 시 HSI 로 복귀 |
| 09 | `12_Advanced_Topics/09_ITM_SWO_Printf_Reg_c` | 레지스터 | (검토 전) ITM/SWO 로 printf (UART 핀 불필요) |
| 10 | `12_Advanced_Topics/10_StackWatermark_Reg_c` | 레지스터 | (검토 전) 스택 페인팅과 하이 워터마크, 재귀 실험 |
| 11 | `12_Advanced_Topics/11_IAP_Bootloader_Reg_c` | 레지스터 | (검토 전) IAP 부트로더: 앱 유효성 검사 후 VTOR 교체·점프 (링커 스크립트 포함) |
| 12 | `12_Advanced_Topics/12_IAP_App_Reg_c` | 레지스터 | (검토 전) 0x08008000 에서 도는 IAP 응용 프로그램 (부트로더와 짝) |
| 13 | `12_Advanced_Topics/13_IAP_Xmodem_Bootloader_Reg_c` | 레지스터 | (검토 전) UART XMODEM-CRC 로 펌웨어 수신·Flash 기록·점프하는 부트로더 |
| 14 | `12_Advanced_Topics/14_OptionBytes_Read_HAL_c` | HAL, FLASH | (검토 전) 옵션 바이트 읽기(RDP/WRP/USER), 읽기 전용 |
| 15 | `12_Advanced_Topics/15_CAN_Normal_2boards_HAL_c` | HAL, CAN | (검토 전) 실제 CAN 버스(2보드): 오류 카운터, 버스오프 복구 |

예제는 기능별 그룹 폴더 아래에 있고, **예제 폴더 번호는 그룹 안에서 항상 `01`부터 순서대로** 붙인다. 폴더명 끝의 `_c`는 Claude가 작성한 예제(검토 전)를 뜻한다. `12_Advanced_Topics`는 3학년 2학기 기본 진도를 넘어서는 **심화(선택)** 예제만 모아 둔 그룹이며, 나머지 01~11 그룹이 한 학기 분량이다. 그룹별 안내는 [Examples/README.md](Examples/README.md) 참고.

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

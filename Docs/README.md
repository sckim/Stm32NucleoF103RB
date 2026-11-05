# Docs — STM32 Nucleo-F103RB 참고 문서

Nucleo-F103RB(MCU **STM32F103RB**, Cortex-M3, 72MHz, Flash 128KB / SRAM 20KB, 중밀도(medium-density))를 다루는 데 필요한 공식 문서 모음이다.
이 폴더의 PDF 표지·목차를 직접 확인해 정리했으며, 쪽수는 이 저장소에 들어 있는 파일 기준이다.

> 처음 시작하는 순서: **[UM1724](UM1724_User_Manual_STM32Nucleo64.pdf)**(보드) + [회로도](Schematic_STM32F103RB.pdf) → **[Datasheet](Datasheet_stm32f103.pdf)**(칩 사양) → **[RM0008](RM0008_Reference_manual.pdf)**(레지스터) → 필요할 때 **[PM0056](PM0056_CortexM3_Programming_Manual.pdf)**(코어) / **[UM1850](HAL_UM1850_hal_and_lowlayer_drivers.pdf)**(HAL) / **[UM1718](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx.pdf)**(CubeMX).  
> 이상한 동작을 만나면 **[ES096](ES096_F103x8xB_Errata.pdf)**(errata)부터 확인한다. 주제를 깊이 볼 때는 응용 노트: **[ADC](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2834_ADC_Accuracy.pdf)**, **[타이머 개요](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4013_Timers_Introduction.pdf)**, **[범용 타이머](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4776_General_Purpose_Timer.pdf)**, **[DMA](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2548_DMA_Controller.pdf)**.

## 1. 문서 목록

| 문서 | 내용 |
|---|---|
| [UM1724](UM1724_User_Manual_STM32Nucleo64.pdf) | Nucleo-64(MB1136) 보드 사용자 매뉴얼. 점퍼(JP), 솔더 브리지(SB), 클럭/USART 배선, 커넥터 핀 배치 |
| [DS5319 (데이터시트)](Datasheet_stm32f103.pdf) | STM32F103x8/xB(64/128KB Flash) 데이터시트(DS5319): 핀 배치, 메모리 맵, 전기적 특성, 패키지 |
| [RM0008](RM0008_Reference_manual.pdf) | STM32F10xxx **레퍼런스 매뉴얼**: 모든 주변장치와 레지스터 상세 |
| [PM0056](PM0056_CortexM3_Programming_Manual.pdf) | Cortex-M3 **프로그래밍 매뉴얼**: 명령어 집합, NVIC, SCB, SysTick, 폴트 |
| [UM1850](HAL_UM1850_hal_and_lowlayer_drivers.pdf) | STM32F1 **HAL/LL 드라이버** API 설명 |
| [MB1136 회로도](Schematic_STM32F103RB.pdf) | 보드(MB1136) 회로도: 개요, 전원, MCU, ST-LINK/V2-1, 확장 커넥터 |
| [UM1718 (st.com)](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx.pdf) | **STM32CubeMX 사용자 매뉴얼**: `.ioc` 설정, 핀/클럭/주변장치 구성, 코드 생성과 `USER CODE` 영역 보존 규칙. 이 저장소 예제 대부분의 생성 도구 |
| [ES096](ES096_F103x8xB_Errata.pdf) | STM32F101/102/103 **x8/xB**(중밀도, F103RB 포함) **디바이스 errata**: 실리콘 결함과 우회 방법, 문서 오류 |
| [AN2834](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2834_ADC_Accuracy.pdf) | **ADC 정확도** 최적화(“How to optimize the ADC accuracy in the STM32 MCUs”, 구 제목 “How to get the best ADC accuracy”): 오차 종류, 샘플 시간과 소스 임피던스, 기준전압·잡음·배선, 교정과 평균. `03_ADC` 예제의 근거 |
| [AN4013](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4013_Timers_Introduction.pdf) | **STM32 타이머 개요**: 범용·고급 타이머 기능, 타임 베이스, 출력 비교·PWM, 입력 캡처, 마스터/슬레이브(TRGO) 연결. `06_Timers_RTC` 예제의 근거 |
| [AN4776](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN4776_General_Purpose_Timer.pdf) | **범용 타이머 활용**(“How to use general-purpose timer peripheral on STM32 MCUs”, 구 제목 “General-purpose timer cookbook”): 주기·펄스 생성, 입력 캡처, 원 펄스, 엔코더, 타이머 연결. 예제 코드는 X-CUBE-TIMCOOKER |
| [AN2548](https://github.com/sckim/Stm32NucleoF103RB/blob/main/Docs/AN2548_DMA_Controller.pdf) | **DMA 컨트롤러** 사용법(F0/F1/F3/Gx/Lx): 채널·요청 매핑, 원형 모드, 우선순위 중재, 버스 대역폭과 지연. `07_DMA` 및 ADC·USART·SPI DMA 예제의 근거 |
| [AN2586](AN2586_Hardware_Development_Getting_Started.pdf) | STM32F10xxx **하드웨어 설계** 입문: 전원, 클럭, 리셋, 부트 모드, 디버그 |
| [AN2606](AN2606_System_Memory_Boot_Mode.pdf) | **시스템 메모리 부트로더** (USART/CAN/USB 등으로 Flash 다운로드). 전 STM32 시리즈 공통 문서 |
| [DB2196](DB2196_Nucleo_64_boards.pdf) | Nucleo-64 **전 제품군** 데이터브리프(F103RB 포함). 보드 특징 요약 |
| [UM1727](UM1727_Getting_started.pdf) | Nucleo 소프트웨어 개발 도구(IAR, Keil, TrueSTUDIO, SW4STM32) 시작 가이드 |
| [NUCLEO-F103RB 제품 페이지 (st.com)](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196) | ST 공식 Nucleo-F103RB 제품 페이지. 이 폴더의 `NUCLEO-F103RB.url`은 같은 주소의 바로가기 파일이다 |

## 2. 이럴 때는 이 문서

| 알고 싶은 것 | 볼 문서 |
|---|---|
| 보드의 LED/버튼/USART 핀, 점퍼, 솔더 브리지 | [UM1724](UM1724_User_Manual_STM32Nucleo64.pdf) (7장 하드웨어 배치와 설정), [회로도](Schematic_STM32F103RB.pdf) |
| 특정 핀의 대체 기능(AF)과 핀 배치, 전기적 한계값 | [Datasheet](Datasheet_stm32f103.pdf) (핀 배치, 핀 정의 표) |
| 레지스터 이름·비트 의미, 주변장치 동작 원리 | [RM0008](RM0008_Reference_manual.pdf) (해당 주변장치 장) |
| 인터럽트 우선순위, SysTick, SCB, 폴트 레지스터 | [PM0056](PM0056_CortexM3_Programming_Manual.pdf) (2장 코어, 4장 코어 주변장치) |
| `HAL_xxx_Init()`, 콜백, LL API 의 사용법 | [UM1850](HAL_UM1850_hal_and_lowlayer_drivers.pdf) |
| 전원/클럭/리셋/부트 핀 하드웨어 설계 | [AN2586](AN2586_Hardware_Development_Getting_Started.pdf) |
| 시스템 메모리 부트로더로 UART/USB 다운로드 | [AN2606](AN2606_System_Memory_Boot_Mode.pdf) |
| 레퍼런스 매뉴얼대로 했는데 동작이 다를 때 (실리콘 결함) | [ES096](ES096_F103x8xB_Errata.pdf) |

### Datasheet 와 Reference Manual 의 역할 구분
- **Datasheet(DS5319)**: 이 칩에 **무엇이 있는지**(주변장치 종류·개수), 어느 핀에 무엇이 나오는지, 전기적 한계가 어떤지를 다룬다.
- **Reference Manual(RM0008)**: 그 주변장치를 **어떤 레지스터로 어떻게 동작시키는지**를 다룬다. 레지스터 수준 코드의 근거가 되는 문서다.
- **이름 혼동 주의**: 흔히 말하는 "user guide" 역할은 **Reference Manual(RM0008)** 이 한다. 이름이 *User Manual* 인 **UM1724 는 Nucleo 보드** 설명서이고, **UM1850 은 HAL/LL API** 설명서다.
- **판단 순서**: RM0008 은 STM32F10x 전 계열(저/중/고/XL 밀도, 연결형)을 함께 다룬다. 그래서 RM0008 에 설명이 있어도 F103RB 에 그 장치가 있다는 보장은 없다. **있는지 여부는 Datasheet**(Table 2 *medium-density device features and peripheral counts*, p.10)로 먼저 확인하고, **사용법은 RM0008** 에서 찾는다.
- **클럭**: 최대 주파수와 발진기 특성 같은 전기적 한계는 Datasheet 에서, PLL·프리스케일러 설정 비트는 RM0008 7장(RCC)에서 본다. 클럭 트리 그림은 두 문서에 모두 있다.

### 블록도는 어디에 있나
블록도는 문서마다 보여 주는 수준이 다르다. 수업에서는 **칩 전체(Datasheet) → 버스 구조(RM0008 3장) → 주변장치 내부(RM0008 각 장)** 순서로 내려가며 보면 좋다.

| 수준 | 그림 | 문서 / 쪽 |
|---|---|---|
| 보드 전체 | Figure 2. Hardware block diagram (MCU–ST-LINK–커넥터) | [UM1724](UM1724_User_Manual_STM32Nucleo64.pdf) p.13 |
| 칩 전체 | Figure 1. STM32F103xx performance line block diagram | [Datasheet](Datasheet_stm32f103.pdf) p.11 |
| 클럭 트리 | Figure 2. Clock tree | [Datasheet](Datasheet_stm32f103.pdf) p.12 |
| 메모리 맵 | Figure 11. Memory map | [Datasheet](Datasheet_stm32f103.pdf) p.34 |
| 핀 정의 | Table 5. Medium-density STM32F103xx pin definitions (AF/리맵 포함) | [Datasheet](Datasheet_stm32f103.pdf) p.28 |
| 버스 구조 | Figure 1. System architecture (low-, medium-, XL-density) — 버스 매트릭스, AHB/APB1/APB2 | [RM0008](RM0008_Reference_manual.pdf) p.47 |
| 주변장치 주소 | Table 3. Register boundary addresses | [RM0008](RM0008_Reference_manual.pdf) p.50 |
| 클럭 트리(설정용) | Figure 8. Clock tree | [RM0008](RM0008_Reference_manual.pdf) p.93 |
| GPIO 핀 내부 | Figure 13/14. Basic structure of a standard / 5 V tolerant I/O port bit | [RM0008](RM0008_Reference_manual.pdf) p.160 |
| EXTI | Figure 20. External interrupt/event controller block diagram | [RM0008](RM0008_Reference_manual.pdf) p.207 |
| ADC | Figure 22. Single ADC block diagram | [RM0008](RM0008_Reference_manual.pdf) p.217 |
| DMA | Figure 49. DMA block diagram (low-, medium-, high-, XL-density) | [RM0008](RM0008_Reference_manual.pdf) p.276 |
| TIM1 | Figure 52. Advanced-control timer block diagram | [RM0008](RM0008_Reference_manual.pdf) p.294 |
| TIM2~4 | Figure 100. General-purpose timer block diagram | [RM0008](RM0008_Reference_manual.pdf) p.367 |
| RTC | Figure 179. RTC simplified block diagram | [RM0008](RM0008_Reference_manual.pdf) p.484 |
| SPI | Figure 238. SPI block diagram | [RM0008](RM0008_Reference_manual.pdf) p.702 |
| I2C | Figure 270. I2C block diagram | [RM0008](RM0008_Reference_manual.pdf) p.755 |
| USART | Figure 279. USART block diagram | [RM0008](RM0008_Reference_manual.pdf) p.789 |

쪽 번호는 각 문서 목차에 적힌 번호다(3장 표와 같은 기준).

## 3. [RM0008](RM0008_Reference_manual.pdf) 장(chapter) 지도 — F103RB 기준

F103RB 는 중밀도 소자이다. 데이터시트(DS5319) 표지 요약: **타이머 7개**(범용 TIM2~4 3개 + 고급 TIM1 1개 + 워치독 IWDG/WWDG 2개 + SysTick 1개, RTC 별도), **12비트 ADC 2개**(ADC1/ADC2가 외부 채널 16개를 공유, 1µs 변환 + ADC1 전용 내부 채널 2개: 온도 센서 CH16, Vrefint CH17), **통신 인터페이스 9개**(USART 최대 3, I2C 최대 2, SPI 최대 2, CAN, USB), **7채널 DMA**. DAC/SDIO/FSMC 는 이 소자의 기능 목록에 없다.
RM0008 은 여러 밀도를 한 문서에 담아서 **F103RB 에는 없는 장**이 섞여 있다.

| 장 | 제목 | 쪽 | F103RB | 관련 예제 (`../Examples/…`) |
|---|---|---|---|---|
| 3 | 메모리와 버스 구조 (3.3.2 비트 밴딩 p.53, 3.3.3 Flash, 3.4 부트 설정) | 47 | ✔ | [09_BitBanding](../Examples/01_GPIO/09_BitBandingReg), [01_Flash_Write](../Examples/10_Flash_CRC/01_Flash_Write_c), [11_IAP_Bootloader](../Examples/12_Advanced_Topics/11_IAP_Bootloader_Reg_c), [12_IAP_App](../Examples/12_Advanced_Topics/12_IAP_App_Reg_c), [14_OptionBytes_Read](../Examples/12_Advanced_Topics/14_OptionBytes_Read_c), [13_IAP_Xmodem_Bootloader](../Examples/12_Advanced_Topics/13_IAP_Xmodem_Bootloader_Reg_c) |
| 4 | CRC 계산 유닛 | 63 | ✔ | [02_CRC_Unit](../Examples/10_Flash_CRC/02_CRC_Unit_c) |
| 5 | 전원 제어(PWR): Sleep p.73 / Stop p.74 / Standby p.76 | 67 | ✔ | [03_Sleep_Mode](../Examples/09_WatchDog_Sleep/03_Sleep_Mode_c), [04_Stop_Mode_EXTI](../Examples/09_WatchDog_Sleep/04_Stop_Mode_EXTI_c), [05_Standby_RTC_Wakeup](../Examples/09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c) |
| 6 | 백업 레지스터(BKP), 6.3.1 탬퍼 p.82 | 81 | ✔ | [05_Standby_RTC_Wakeup](../Examples/09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c), [13_RTC_Calendar](../Examples/06_Timers_RTC/13_RTC_Calendar_c), [06_BKP_Tamper](../Examples/09_WatchDog_Sleep/06_BKP_Tamper_c) |
| 7 | 리셋과 클럭 제어(RCC), 7.2.7 CSS p.97, 7.3.2 RCC_CFGR p.101 | 90 | ✔ | [04_Clock_Config](../Examples/08_Clock_System/04_Clock_Config_Reg_c), [09_MCO_ClockOut](../Examples/08_Clock_System/09_MCO_ClockOut_c), [08_HSE_PLL_72MHz](../Examples/12_Advanced_Topics/08_HSE_PLL_72MHz_Reg_c) |
| 8 | 연결형(F105/F107) RCC | 123 | ✘ | — |
| 9 | GPIO와 대체 기능(AFIO): 9.3 AFIO p.175, 9.4.2 AFIO_MAPR p.184 | — | ✔ | [01_GPIO 전체](../Examples/01_GPIO), [07_AFIO_Remap](../Examples/08_Clock_System/07_AFIO_Remap_Reg_c) |
| 10 | 인터럽트와 이벤트(EXTI) | 197 | ✔ | [06_ExtInt](../Examples/01_GPIO/06_ExtInt), [08_ExternalInt](../Examples/01_GPIO/08_ExternalInt), [06_NVIC_Priority](../Examples/08_Clock_System/06_NVIC_Priority_c) |
| 11 | ADC: 11.3.7 아날로그 워치독 p.220, 11.3.9 주입 채널 p.221, 11.9.2 정규 동시 모드 p.230, 11.10 온도 센서 p.235 | 215 | ✔ | [03_ADC](../Examples/03_ADC) (01~06), [01_ADC_Injected](../Examples/12_Advanced_Topics/01_ADC_Injected_c), [02_ADC_DualMode](../Examples/12_Advanced_Topics/02_ADC_DualMode_c) |
| 12 | DAC | 254 | ✘ (대용량 이상) | — |
| 13 | DMA: 13.3.2 중재기(Arbiter) p.277, 13.3.7 요청 매핑 p.281 | 274 | ✔ (DMA1, 7채널) | [01_DMA_MemToMem](../Examples/07_DMA/01_DMA_MemToMem), [04_USART_DMA_IdleLine](../Examples/02_USART/04_USART_DMA_IdleLine_c), [04_ADC_DMA_TimTrigger](../Examples/03_ADC/04_ADC_DMA_TimTrigger), [02_SPI_DMA_Fullduplex](../Examples/05_SPI_Communication/02_SPI_DMA_Fullduplex_c), [02_DMA_Circular_GPIO](../Examples/07_DMA/02_DMA_Circular_GPIO), [06_DMA_Priority](../Examples/12_Advanced_Topics/06_DMA_Priority_c), [05_I2C_MPU6050_DMA](../Examples/04_I2C_Communication/05_I2C_MPU6050_DMA_c) |
| 14 | 고급 제어 타이머 (TIM1) | 292 | ✔ (TIM1만; TIM8 없음) | [08_TIM1_DeadTime_Break](../Examples/06_Timers_RTC/08_TIM1_DeadTime_Break_c) |
| 15 | 범용 타이머 (TIM2~TIM5): 15.3.6 PWM 입력 p.385, 15.3.10 원 펄스 p.390, 15.3.12 엔코더 p.392, 15.3.15 동기화 p.398 | 365 | ✔ (TIM2~4만; TIM5 없음) | [06_Timers_RTC](../Examples/06_Timers_RTC) (01~03, 06~09), [05_TIM_Cascade32](../Examples/12_Advanced_Topics/05_TIM_Cascade32_c) |
| 16 | 범용 타이머 (TIM9~TIM14) | 425 | ✘ | — |
| 17 | 기본 타이머 (TIM6, TIM7) | 469 | ✘ (대용량 이상) | — |
| 18 | RTC | 482 | ✔ | [13_RTC_Calendar](../Examples/06_Timers_RTC/13_RTC_Calendar_c), [05_Standby_RTC_Wakeup](../Examples/09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c) |
| 19 | 독립 워치독 (IWDG) | 494 | ✔ | [01_WatchdogTimer](../Examples/09_WatchDog_Sleep/01_WatchdogTimer), [07_IWDG](../Examples/09_WatchDog_Sleep/07_IWDG_c) |
| 20 | 윈도우 워치독 (WWDG) | 500 | ✔ | [02_WWDG](../Examples/09_WatchDog_Sleep/02_WWDG_c) |
| 21 | FSMC | 507 | ✘ | — |
| 22 | SDIO | 566 | ✘ | — |
| 23 | USB 풀스피드 디바이스 | 622 | ✔ (Nucleo 에 커넥터 없음, PA11/PA12 배선 필요) | — |
| 24 | bxCAN (24.7.4 필터 p.664) | 653 | ✔ (Nucleo 에 트랜시버 없음) | [01_CAN_Loopback](../Examples/11_CAN_Communication/01_CAN_Loopback_c), [02_CAN_Filter](../Examples/11_CAN_Communication/02_CAN_Filter_c), [15_CAN_Normal_2boards](../Examples/12_Advanced_Topics/15_CAN_Normal_2boards_c) |
| 25 | SPI | 699 | ✔ (SPI1, SPI2) | [05_SPI_Communication](../Examples/05_SPI_Communication) |
| 26 | I2C | 752 | ✔ (I2C1, I2C2) | [04_I2C_Communication](../Examples/04_I2C_Communication) |
| 27 | USART | — | ✔ (USART1~3) | [02_USART](../Examples/02_USART) |
| 28 | USB OTG FS | 828 | ✘ (연결형) | — |
| 29 | 이더넷 (ETH) | — | ✘ (연결형) | — |
| 30 | 디바이스 전자 서명: 30.2 96비트 고유 ID p.1077 | 1076 | ✔ | [12_DeviceInfo_UniqueID](../Examples/08_Clock_System/12_DeviceInfo_UniqueID_Reg_c) |
| 31 | 디버그 지원(DBG): 31.16.1 저전력 모드 디버그 p.1100 | 1079 | ✔ | [03_Sleep_Mode](../Examples/09_WatchDog_Sleep/03_Sleep_Mode_c), [04_Stop_Mode_EXTI](../Examples/09_WatchDog_Sleep/04_Stop_Mode_EXTI_c), [09_ITM_SWO_Printf](../Examples/12_Advanced_Topics/09_ITM_SWO_Printf_Reg_c) |

"쪽"은 PDF 상의 쪽 번호이고, `—` 는 목차에서 확인하지 못한 항목이다. 예제의 소스 주석에도 참조한 장/절을 적어 두었다.

## 4. [PM0056](PM0056_CortexM3_Programming_Manual.pdf) (Cortex-M3) 절 지도

| 절 | 내용 | 쪽 | 관련 예제 |
|---|---|---|---|
| 2.1.1 / 2.1.3 | 프로세서 모드·권한 / 코어 레지스터(R0~R15, xPSR, CONTROL) | 13 | [01_CoreRegisters_Instructions](../Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) |
| 2.2.1 / 2.2.6 | 메모리 영역(Code/SRAM/Peripheral/PPB) / 리틀 엔디안 | 24 / — | [02_MemoryMap](../Examples/08_Clock_System/02_MemoryMap_Reg_c) |
| 2.2.5 | 비트 밴딩 | 27 | [09_BitBanding](../Examples/01_GPIO/09_BitBandingReg), [02_MemoryMap](../Examples/08_Clock_System/02_MemoryMap_Reg_c) |
| 2.3.4 | 벡터 테이블 | — | [03_ExceptionModel_VectorTable](../Examples/08_Clock_System/03_ExceptionModel_VectorTable_Reg_c) |
| 2.3.6 | 인터럽트 우선순위 그룹핑 | 36 | [06_NVIC_Priority](../Examples/08_Clock_System/06_NVIC_Priority_c) |
| 2.3.7 | 예외 진입/복귀, EXC_RETURN(Table 17) | — | [03_ExceptionModel_VectorTable](../Examples/08_Clock_System/03_ExceptionModel_VectorTable_Reg_c) |
| 2.4.3 | 폴트 상태/주소 레지스터 | 41 | [08_HardFault_Diagnosis](../Examples/08_Clock_System/08_HardFault_Diagnosis_Reg_c) |
| 3.4.7 | PUSH/POP | — | [01_CoreRegisters_Instructions](../Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) |
| 3.5.3 | 조건 플래그 N Z C V | — | [01_CoreRegisters_Instructions](../Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) |
| 3.6.3 | SDIV/UDIV (하드웨어 나눗셈) | — | [01_CoreRegisters_Instructions](../Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) |
| 3.8.7 | IT (조건부 실행) | — | [01_CoreRegisters_Instructions](../Examples/08_Clock_System/01_CoreRegisters_Instructions_Reg_c) |
| 4.3 / 4.3.7 | NVIC / 우선순위 레지스터(NVIC_IPR) | 118 / 125 | [06_NVIC_Priority](../Examples/08_Clock_System/06_NVIC_Priority_c) |
| 4.4.4 | SCB_VTOR (벡터 테이블 오프셋) | 133 | [11_IAP_Bootloader](../Examples/12_Advanced_Topics/11_IAP_Bootloader_Reg_c), [12_IAP_App](../Examples/12_Advanced_Topics/12_IAP_App_Reg_c) |
| 4.4.6 | SCB_SCR (SLEEPDEEP, SLEEPONEXIT) | 136 | [03_Sleep_Mode](../Examples/09_WatchDog_Sleep/03_Sleep_Mode_c), [04_Stop_Mode_EXTI](../Examples/09_WatchDog_Sleep/04_Stop_Mode_EXTI_c) |
| 4.4.10 / 4.4.11 | SCB_CFSR / SCB_HFSR (폴트 원인) | 142 / 145 | [08_HardFault_Diagnosis](../Examples/08_Clock_System/08_HardFault_Diagnosis_Reg_c) |
| 4.5 | SysTick (STK_CTRL/LOAD/VAL) | 150 | [05_SysTick](../Examples/08_Clock_System/05_SysTick_Reg_c), [07_PendSV_ContextSwitch](../Examples/12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c) |

## 5. Nucleo-F103RB 보드 핵심 정보 ([UM1724](UM1724_User_Manual_STM32Nucleo64.pdf) 기준)

| 항목 | 내용 |
|---|---|
| 사용자 LED / 버튼 | LD2 = **PA5**, B1 = **PC13** (눌리면 Low, 보드 외부 풀업) |
| 가상 COM 포트 | USART2 **PA2(TX)/PA3(RX)** 가 ST-LINK 에 연결됨 (SB13, SB14 ON, SB62/SB63 OFF 가 기본). 아두이노/모르포 핀으로 쓰려면 브리지 변경 |
| 소비 전류 측정 | **JP6 (IDD)** 점퍼를 빼고 전류계를 연결 (MCU 전류만 측정) |
| 디버거 | 온보드 ST-LINK/V2-1 (SWD). 별도 프로브 불필요 |
| 커넥터 | ARDUINO Uno V3 커넥터 + ST morpho 커넥터 (CN7, CN10) |

### 클럭 소스는 보드 개정(MB1136 C-0x)에 따라 기본 구성이 다르다
보드 뒷면 스티커에서 개정(예: `MB1136 C-03`)을 확인한다.

| 클럭 | C-01 | C-02 | C-03 이상 |
|---|---|---|---|
| HSE (8MHz) | 사용 안 함 (PF0/PF1 이 GPIO) | **ST-LINK MCO 를 HSE 입력**으로 사용 (8MHz 고정, SB54/SB16/SB50 ON) | C-02 와 동일 |
| LSE (32.768kHz) | 사용 안 함 (PC14/PC15 가 GPIO) | 온보드 X2 크리스털 | 새 LSE 크리스털(ABS25) + 부하 커패시터 값 변경 |

이 차이가 이 저장소 예제에 영향을 주는 곳:
- **HSE**: 모든 HAL 예제는 **HSI/2 × 16 = 64MHz** 를 쓰므로 개정과 무관하게 동작한다. HSE 는 [09_MCO_ClockOut](../Examples/08_Clock_System/09_MCO_ClockOut_c) 에서 **바이패스 모드**로만 시도하며, C-01 처럼 HSE 가 없으면 그 소스는 자동으로 건너뛴다.
- **LSE**: [13_RTC_Calendar](../Examples/06_Timers_RTC/13_RTC_Calendar_c) 와 [05_Standby_RTC_Wakeup](../Examples/09_WatchDog_Sleep/05_Standby_RTC_Wakeup_c) 는 LSE 시작에 실패하면 **LSI 로 자동 대체**한다. (LSI 는 정확도가 낮아 시계가 빠르게 어긋난다)
- 외부 8MHz 크리스털(X3)은 기본 미장착이라 직접 납땜해야 한다 (사양: 8MHz, 16pF, 20ppm, DIP).

## 6. 이 폴더와 다른 곳의 관계

- 저장소 루트의 [README.md](../README.md) 는 전체 구조와 예제 표를, 이 문서는 **참고 문서의 상세**를 다룬다.
- 예제 폴더는 기능별 그룹으로 정리되어 있다: [Examples/](../Examples).

---

### 사용할 때 알아 둘 점
- **ES096 은 F103RB 전용 errata** 다. 적용 대상은 STM32F103x8/xB 이며, 모든 실리콘 리비전(A, B, Z, Y/X/1/2/3)에 걸쳐 결함 여부를 표로 보여 준다. 예제와 직접 관련된 항목:
  - **2.2.2 디버그 레지스터를 사용자 소프트웨어가 읽을 수 없음 (우회 방법 없음)**: `DBGMCU_IDCODE` 와 `DBGMCU_CR` 는 디버그 모드에서만 읽히고, 디버거 없이 실행하면 **0x00** 이 읽힌다. [12_DeviceInfo_UniqueID](../Examples/08_Clock_System/12_DeviceInfo_UniqueID_Reg_c) 의 `DEV_ID`/`REV_ID` 는 디버거를 연결한 상태에서만 정상값(0x410)이 나온다.
  - **2.2.3 Stop 모드 디버그와 SysTick**: `DBG_STOP` 을 켜고 Stop 모드를 디버그하면 SysTick 인터럽트가 MCU 를 깨운다. Stop 모드를 디버그할 때는 SysTick 인터럽트를 끈다 ([04_Stop_Mode_EXTI](../Examples/09_WatchDog_Sleep/04_Stop_Mode_EXTI_c)).
  - **2.8.x I2C**: 이벤트(EV7, EV6_1 등)를 제때 처리하지 못하면 바이트가 더 받아지거나 빠질 수 있다. DMA 를 쓰거나 I2C 인터럽트 우선순위를 가장 높게 두는 것이 권장 우회 방법이다.
- **UM1727 은 오래된 문서(2016)** 라서 소개하는 IDE(TrueSTUDIO, SW4STM32)는 현재 지원이 종료되었다. 빌드/디버그 흐름은 이 저장소의 PlatformIO 또는 STM32CubeIDE 기준으로 보고, UM1727 은 ST-LINK 드라이버 설치와 Nucleo 개념 이해용으로만 참고한다.
- **AN2606 은 전 시리즈 공통 문서(553쪽)** 다. F103 부분(STM32F10xxx 부트로더 절)만 찾아 읽으면 된다.
- **DB2196 은 모든 Nucleo-64 보드용** 이라 F103RB 만의 상세 정보는 UM1724 와 회로도가 정확하다.
- **시판 교재(Joseph Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*)는 저작권 자료라 저장소에 포함하지 않는다.**
- 문서 개정은 ST 사이트에서 계속 올라가므로, 레지스터 값 등 중요한 내용은 최신 개정을 참고할 필요가 있다.

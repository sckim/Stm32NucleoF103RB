# Docs — STM32 Nucleo-F103RB 참고 문서

Nucleo-F103RB(MCU **STM32F103RB**, Cortex-M3, 72MHz, Flash 128KB / SRAM 20KB, 중밀도(medium-density))를 다루는 데 필요한 공식 문서 모음이다.
이 폴더의 PDF 표지·목차를 직접 확인해 정리했으며, 쪽수와 개정은 이 저장소에 들어 있는 파일 기준이다.

> 처음 시작하는 순서: **UM1724**(보드) → **Datasheet**(칩 사양) → **RM0008**(레지스터) → 필요할 때 **PM0056**(코어) / **UM1850**(HAL).

## 1. 문서 목록

| 파일 | 문서 | 개정 / 분량 | 내용 |
|---|---|---|---|
| [UM1724_User_Manual_STM32Nucleo64.pdf](UM1724_User_Manual_STM32Nucleo64.pdf) | UM1724 | Rev 17 (2025-09) / 91쪽 | Nucleo-64(MB1136) 보드 사용자 매뉴얼. 점퍼(JP), 솔더 브리지(SB), 클럭/USART 배선, 커넥터 핀 배치 |
| [Schematic_STM32F103RB.pdf](Schematic_STM32F103RB.pdf) | MB1136 회로도 | 5장(sheet) | 보드 회로도: 개요, 전원, MCU, ST-LINK/V2-1, 확장 커넥터 |
| [DB2196_Nucleo_64_boards.pdf](DB2196_Nucleo_64_boards.pdf) | DB2196 | Rev 22 (2026-02) / 13쪽 | Nucleo-64 **전 제품군** 데이터브리프(F103RB 포함). 보드 특징 요약 |
| [Datasheet_stm32f103.pdf](Datasheet_stm32f103.pdf) | DS5319 | Rev 19 (2023-09) / 114쪽 | STM32F103x8/xB(64/128KB Flash) 데이터시트: 핀 배치, 메모리 맵, 전기적 특성, 패키지 |
| [RM0008_Reference_manual.pdf](RM0008_Reference_manual.pdf) | RM0008 | Rev 21 / 1136쪽 | STM32F10xxx **레퍼런스 매뉴얼**: 모든 주변장치와 레지스터 상세 |
| [PM0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf](<PM0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf>) | PM0056 | Rev 7 (2024-12) / 156쪽 | Cortex-M3 **프로그래밍 매뉴얼**: 명령어 집합, NVIC, SCB, SysTick, 폴트 |
| [HAL_UM1850_hal_and_lowlayer_drivers.pdf](HAL_UM1850_hal_and_lowlayer_drivers.pdf) | UM1850 | Rev 3 (2020-02) / 1208쪽 | STM32F1 **HAL/LL 드라이버** API 설명 |
| [AN2586-getting-started-with-stm32f10xxx-hardware-development-stmicroelectronics.pdf](<AN2586-getting-started-with-stm32f10xxx-hardware-development-stmicroelectronics.pdf>) | AN2586 | Rev 8 (2022-12) / 29쪽 | STM32F10xxx **하드웨어 설계** 입문: 전원, 클럭, 리셋, 부트 모드, 디버그 |
| [AN2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf](<AN2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf>) | AN2606 | Rev 70 (2026-02) / 553쪽 | **시스템 메모리 부트로더** (USART/CAN/USB 등으로 Flash 다운로드). 전 STM32 시리즈 공통 문서 |
| [UM1727_Getting_started.pdf](UM1727_Getting_started.pdf) | UM1727 | Rev 5 (2016-01) / 22쪽 | Nucleo 소프트웨어 개발 도구(IAR, Keil, TrueSTUDIO, SW4STM32) 시작 가이드 |
| [ES0340-stm32f101xcde-stm32f103xcde-device-errata-stmicroelectronics.pdf](<ES0340-stm32f101xcde-stm32f103xcde-device-errata-stmicroelectronics.pdf>) | ES0340 | Rev 17 (2022-06) / 38쪽 | STM32F101/103 **xC/D/E** 디바이스 errata (아래 주의 참고) |
| [NUCLEO-F103RB.url](NUCLEO-F103RB.url) | — | — | ST 공식 [Nucleo-F103RB 제품 페이지](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196) 바로가기 |

### 사용할 때 알아 둘 점
- **ES0340 은 F103RB 에 적용되지 않는다.** 표지의 적용 대상이 STM32F101/103 **xC/D/E**(대용량) 부품번호(RC, RD, RE, VC…ZE)이고, F103RB 는 **xB**(중밀도)이다. F103RB 의 errata 는 ST 사이트의 STM32F103x8/xB 전용 errata 시트를 따로 받아서 확인해야 한다.
- **UM1727 은 오래된 문서(2016)** 라서 소개하는 IDE(TrueSTUDIO, SW4STM32)는 현재 지원이 종료되었다. 빌드/디버그 흐름은 이 저장소의 PlatformIO 또는 STM32CubeIDE 기준으로 보고, UM1727 은 ST-LINK 드라이버 설치와 Nucleo 개념 이해용으로만 참고한다.
- **AN2606 은 전 시리즈 공통 문서(553쪽)** 다. F103 부분(STM32F10xxx 부트로더 절)만 찾아 읽으면 된다.
- **DB2196 은 모든 Nucleo-64 보드용** 이라 F103RB 만의 상세 정보는 UM1724 와 회로도가 정확하다.
- **시판 교재(Joseph Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*)는 저작권 자료라 저장소에 포함하지 않는다.** 필요하면 각자 구해서 이 폴더에 두면 되고, `.gitignore` 로 커밋되지 않게 막아 두었다.
- 문서 개정은 ST 사이트에서 계속 올라가므로, 레지스터 값 등 중요한 내용은 최신 개정과 대조한다.

## 2. 이럴 때는 이 문서

| 알고 싶은 것 | 볼 문서 |
|---|---|
| 보드의 LED/버튼/USART 핀, 점퍼, 솔더 브리지 | UM1724 (7장 하드웨어 배치와 설정), 회로도 |
| 특정 핀의 대체 기능(AF)과 핀 배치, 전기적 한계값 | Datasheet (핀 배치, 핀 정의 표) |
| 레지스터 이름·비트 의미, 주변장치 동작 원리 | RM0008 (해당 주변장치 장) |
| 인터럽트 우선순위, SysTick, SCB, 폴트 레지스터 | PM0056 (2장 코어, 4장 코어 주변장치) |
| `HAL_xxx_Init()`, 콜백, LL API 의 사용법 | UM1850 |
| 전원/클럭/리셋/부트 핀 하드웨어 설계 | AN2586 |
| 시스템 메모리 부트로더로 UART/USB 다운로드 | AN2606 |

## 3. RM0008 장(chapter) 지도 — F103RB 기준

F103RB 는 중밀도 소자이다. 데이터시트(DS5319) 표지 요약: **타이머 7개**(16비트 범용 3개 + 모터 제어용 1개 + 워치독 2개 + SysTick), **ADC 2개**, **통신 인터페이스 9개**(USART 최대 3, I2C 최대 2, SPI 최대 2, CAN, USB), **7채널 DMA**. DAC/SDIO/FSMC 는 이 소자의 기능 목록에 없다.
RM0008 은 여러 밀도를 한 문서에 담아서 **F103RB 에는 없는 장**이 섞여 있다.

| 장 | 제목 | 쪽 | F103RB | 관련 예제 (`../Examples/…`) |
|---|---|---|---|---|
| 3 | 메모리와 버스 구조 (3.3.2 비트 밴딩 p.53, 3.3.3 Flash, 3.4 부트 설정) | 47 | ✔ | [08_BitBanding](../Examples/01_GPIO/08_BitBanding_Reg_c), [90_Flash_Write](../Examples/10_Flash_CRC/90_Flash_Write_HAL_c) |
| 4 | CRC 계산 유닛 | 63 | ✔ | [91_CRC_Unit](../Examples/10_Flash_CRC/91_CRC_Unit_HAL_c) |
| 5 | 전원 제어(PWR): Sleep p.73 / Stop p.74 / Standby p.76 | 67 | ✔ | [82_Sleep](../Examples/09_WatchDog_Sleep/82_Sleep_Mode_HAL_c), [83_Stop](../Examples/09_WatchDog_Sleep/83_Stop_Mode_EXTI_HAL_c), [84_Standby](../Examples/09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c) |
| 6 | 백업 레지스터(BKP) | 81 | ✔ | [84_Standby](../Examples/09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c), [54_RTC_Calendar](../Examples/06_Timers_RTC/54_RTC_Calendar_HAL_c) |
| 7 | 리셋과 클럭 제어(RCC), 7.2.7 CSS p.97, 7.3.2 RCC_CFGR p.101 | 90 | ✔ | [70_Clock_Config](../Examples/08_Clock_System/70_Clock_Config_Reg_c), [76_MCO_ClockOut](../Examples/08_Clock_System/76_MCO_ClockOut_HAL_c) |
| 8 | 연결형(F105/F107) RCC | 123 | ✘ | — |
| 9 | GPIO와 대체 기능(AFIO): 9.3 AFIO p.175, 9.4.2 AFIO_MAPR p.184 | — | ✔ | [01_GPIO 전체](../Examples/01_GPIO), [73_AFIO_Remap](../Examples/08_Clock_System/73_AFIO_Remap_Reg_c) |
| 10 | 인터럽트와 이벤트(EXTI) | 197 | ✔ | [05_ExtInt](../Examples/01_GPIO/05_ExtInt), [07_ExternalInt](../Examples/01_GPIO/07_ExternalInt), [72_NVIC_Priority](../Examples/08_Clock_System/72_NVIC_Priority_HAL_c) |
| 11 | ADC: 11.3.7 아날로그 워치독 p.220, 11.10 온도 센서 p.235 | 215 | ✔ | [03_ADC](../Examples/03_ADC) (20~23) |
| 12 | DAC | 254 | ✘ (대용량 이상) | — |
| 13 | DMA: 13.3.7 요청 매핑 p.281 | 274 | ✔ (DMA1, 7채널) | [60_DMA_MemToMem](../Examples/07_DMA/60_DMA_MemToMem_HAL_c), [12](../Examples/02_USART/12_USART_DMA_IdleLine_HAL_c), [21](../Examples/03_ADC/21_ADC_DMA_TimTrigger_HAL_c), [41](../Examples/05_SPI_Communication/41_SPI_DMA_Fullduplex_HAL_c) |
| 14 | 고급 제어 타이머 (TIM1) | 292 | ✔ (TIM1만; TIM8 없음) | [53_TIM1_DeadTime_Break](../Examples/06_Timers_RTC/53_TIM1_DeadTime_Break_HAL_c) |
| 15 | 범용 타이머 (TIM2~TIM5): 15.3.6 PWM 입력 p.385, 15.3.10 원 펄스 p.390, 15.3.12 엔코더 p.392, 15.3.15 동기화 p.398 | 365 | ✔ (TIM2~4만; TIM5 없음) | [50~52, 55~57](../Examples/06_Timers_RTC) |
| 16 | 범용 타이머 (TIM9~TIM14) | 425 | ✘ | — |
| 17 | 기본 타이머 (TIM6, TIM7) | 469 | ✘ (대용량 이상) | — |
| 18 | RTC | 482 | ✔ | [54_RTC_Calendar](../Examples/06_Timers_RTC/54_RTC_Calendar_HAL_c), [84_Standby](../Examples/09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c) |
| 19 | 독립 워치독 (IWDG) | 494 | ✔ | [80_WatchdogTimer](../Examples/09_WatchDog_Sleep/80_WatchdogTimer) |
| 20 | 윈도우 워치독 (WWDG) | 500 | ✔ | [81_WWDG](../Examples/09_WatchDog_Sleep/81_WWDG_HAL_c) |
| 21 | FSMC | 507 | ✘ | — |
| 22 | SDIO | 566 | ✘ | — |
| 23 | USB 풀스피드 디바이스 | 622 | ✔ (Nucleo 에 커넥터 없음, PA11/PA12 배선 필요) | — |
| 24 | bxCAN | 653 | ✔ (Nucleo 에 트랜시버 없음) | [100_CAN_Loopback](../Examples/11_CAN_Communication/100_CAN_Loopback_HAL_c) |
| 25 | SPI | 699 | ✔ (SPI1, SPI2) | [05_SPI_Communication](../Examples/05_SPI_Communication) |
| 26 | I2C | 752 | ✔ (I2C1, I2C2) | [04_I2C_Communication](../Examples/04_I2C_Communication) |
| 27 | USART | — | ✔ (USART1~3) | [02_USART](../Examples/02_USART) |
| 28 | USB OTG FS | 828 | ✘ (연결형) | — |
| 29 | 이더넷 (ETH) | — | ✘ (연결형) | — |
| 30 | 디바이스 전자 서명: 30.2 96비트 고유 ID p.1077 | 1076 | ✔ | [79_DeviceInfo_UniqueID](../Examples/08_Clock_System/79_DeviceInfo_UniqueID_Reg_c) |
| 31 | 디버그 지원(DBG): 31.16.1 저전력 모드 디버그 p.1100 | 1079 | ✔ | [82_Sleep](../Examples/09_WatchDog_Sleep/82_Sleep_Mode_HAL_c), [83_Stop](../Examples/09_WatchDog_Sleep/83_Stop_Mode_EXTI_HAL_c) |

"쪽"은 PDF 상의 쪽 번호이고, `—` 는 목차에서 확인하지 못한 항목이다. 예제의 소스 주석에도 참조한 장/절을 적어 두었다.

## 4. PM0056 (Cortex-M3) 절 지도

| 절 | 내용 | 쪽 | 관련 예제 |
|---|---|---|---|
| 2.2.5 | 비트 밴딩 | 27 | [08_BitBanding](../Examples/01_GPIO/08_BitBanding_Reg_c) |
| 2.3.6 | 인터럽트 우선순위 그룹핑 | 36 | [72_NVIC_Priority](../Examples/08_Clock_System/72_NVIC_Priority_HAL_c) |
| 2.4.3 | 폴트 상태/주소 레지스터 | 41 | [74_HardFault_Diagnosis](../Examples/08_Clock_System/74_HardFault_Diagnosis_Reg_c) |
| 4.3 / 4.3.7 | NVIC / 우선순위 레지스터(NVIC_IPR) | 118 / 125 | [72_NVIC_Priority](../Examples/08_Clock_System/72_NVIC_Priority_HAL_c) |
| 4.4.6 | SCB_SCR (SLEEPDEEP, SLEEPONEXIT) | 136 | [82_Sleep](../Examples/09_WatchDog_Sleep/82_Sleep_Mode_HAL_c), [83_Stop](../Examples/09_WatchDog_Sleep/83_Stop_Mode_EXTI_HAL_c) |
| 4.4.10 / 4.4.11 | SCB_CFSR / SCB_HFSR (폴트 원인) | 142 / 145 | [74_HardFault_Diagnosis](../Examples/08_Clock_System/74_HardFault_Diagnosis_Reg_c) |
| 4.5 | SysTick (STK_CTRL/LOAD/VAL) | 150 | [71_SysTick](../Examples/08_Clock_System/71_SysTick_Reg_c), [75_PendSV_ContextSwitch](../Examples/08_Clock_System/75_PendSV_ContextSwitch_Reg_c) |

## 5. Nucleo-F103RB 보드 핵심 정보 (UM1724 기준)

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
- **HSE**: 모든 HAL 예제는 **HSI/2 × 16 = 64MHz** 를 쓰므로 개정과 무관하게 동작한다. HSE 는 [76_MCO_ClockOut](../Examples/08_Clock_System/76_MCO_ClockOut_HAL_c) 에서 **바이패스 모드**로만 시도하며, C-01 처럼 HSE 가 없으면 그 소스는 자동으로 건너뛴다.
- **LSE**: [54_RTC_Calendar](../Examples/06_Timers_RTC/54_RTC_Calendar_HAL_c) 와 [84_Standby_RTC_Wakeup](../Examples/09_WatchDog_Sleep/84_Standby_RTC_Wakeup_HAL_c) 는 LSE 시작에 실패하면 **LSI 로 자동 대체**한다. (LSI 는 정확도가 낮아 시계가 빠르게 어긋난다)
- 외부 8MHz 크리스털(X3)은 기본 미장착이라 직접 납땜해야 한다 (사양: 8MHz, 16pF, 20ppm, DIP).

## 6. 이 폴더와 다른 곳의 관계

- 저장소 루트의 [README.md](../README.md) 는 전체 구조와 예제 표를, 이 문서는 **참고 문서의 상세**를 다룬다.
- 예제 폴더는 기능별 그룹으로 정리되어 있다: [Examples/](../Examples).
- 새 PDF 를 추가할 때는 위 "문서 목록" 표에 **문서 번호, 개정, 쪽수, 한 줄 설명**을 함께 추가한다.

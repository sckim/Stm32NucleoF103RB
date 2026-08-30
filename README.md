# Stm32NucleoF103RB

**임베디드시스템** 교과목의 실습 자료 저장소이다. STM32 Nucleo-F103RB(ARM Cortex-M3) 보드를 기준으로 GPIO부터 인터럽트, 타이머, ADC, UART, I2C까지 단계별 예제 프로젝트와 참고 문서, 회로 시뮬레이션 파일을 정리한다.

## 개발 환경

* IDE: VS Code + [PlatformIO](https://platformio.org/) (예제 대부분), 일부는 STM32CubeIDE 프로젝트 파일(`.project`, `.cproject`, `.ioc`)을 함께 포함함
* 보드 설정: STM32CubeMX로 생성한 `.ioc` 파일 기준
* 드라이버: STM32 HAL 및 LL(Low-Layer) 라이브러리, 일부 예제는 레지스터 직접 제어 방식
* 회로 시뮬레이션: [Proteus](https://www.labcenter.com/) (`STM32F103R.pdsprj`)

## 폴더 구조

| 폴더/파일 | 내용 |
|---|---|
| `Docs/` | 데이터시트, 레퍼런스 매뉴얼 등 STM32F103 관련 공식 문서(PDF) |
| `Examples/` | 주차별 실습 예제 프로젝트 모음 |
| `Project Backups/` | Proteus 시뮬레이션 자동/수동 백업본 |
| `03_STM32.code-workspace` | VS Code 다중 폴더 워크스페이스 설정 |
| `STM32F103R.pdsprj` | Proteus 회로 시뮬레이션 프로젝트(현재본) |

## 실습 예제 (`Examples/`)

| 번호 | 예제 | 구현 방식 | 내용 |
|---|---|---|---|
| 00 | `00_BlinkArduino` | Arduino 프레임워크(PlatformIO) | `pinMode`/`digitalWrite`로 온보드 LED(PA5) 점멸 |
| 01 | `01_BlinkHAL` | HAL 드라이버 | HAL API 기반 LED 점멸 |
| 02 | `02_BlinkLL` | LL 드라이버 | LL API 기반 LED 점멸 |
| 03 | `03_BlinkReg` | 레지스터 직접 제어 | `RCC`, `GPIOx->CRL` 레지스터를 직접 조작하여 LED 점멸, SysTick 기반 딜레이 구현 |
| 04 | `04_ReadPin` | HAL | 사용자 버튼(B1) 폴링 입력으로 LED(LD2) 제어 |
| 05 | `05_ExtInt` | HAL, EXTI | 버튼 입력을 EXTI 인터럽트로 처리(`HAL_GPIO_EXTI_Callback`) |
| 10 | `10_USART_printf` | HAL, UART | `printf` 출력을 USART로 리다이렉션 |
| 30 | `30_ADC_Temperature` | HAL, ADC | 내부 온도 센서 ADC 값을 UART로 출력 |
| 40 | `40_TIM_TimeBase` | HAL, TIM | 타이머 주기 인터럽트(`HAL_TIM_PeriodElapsedCallback`)로 LED 토글 |
| 41 | `41_WatchdogTimer` | HAL, IWDG | 독립 워치독 타이머(IWDG) 동작 확인 |
| - | `PCF8574` | HAL, I2C | PCF8574 I2C 확장 IC를 통한 문자 LCD 제어, I2C 주소 스캐너 포함 |

각 예제 폴더는 PlatformIO 프로젝트(`platformio.ini` 포함) 또는 STM32CubeIDE 프로젝트(`.project`/`.cproject`/`.ioc` 포함) 형태이며, VS Code에서 PlatformIO 확장으로 열어 빌드·업로드하는 것을 기본으로 한다.

## 참고 문서 (`Docs/`)

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
| [The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors.pdf](<Docs/The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors.pdf>) | Cortex-M3/M4 프로세서 해설서(Joseph Yiu) |
| [NUCLEO-F103RB.url](Docs/NUCLEO-F103RB.url) | [ST 공식 제품 페이지](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196) 바로가기 |

## 회로 시뮬레이션
`STM32F103R.pdsprj`는 Proteus용 회로 시뮬레이션 프로젝트

## 관련 교과목

본 저장소는 **임베디드시스템**(전자공학과 3학년 2학기) 교과목의 실습 자료로 사용된다. 강의계획서 및 주차별 진도는 별도 문서로 관리한다.

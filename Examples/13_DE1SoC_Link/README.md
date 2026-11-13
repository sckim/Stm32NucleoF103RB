# 13_DE1SoC_Link — Nucleo-F103RB + DE1-SoC(FPGA) 연동 실습

STM32 Nucleo-F103RB(MCU)와 DE1-SoC(FPGA)를 점퍼선으로 연결해, MCU가 만든 신호를 FPGA가 관찰·해석·측정·계수하는 실습 네 개를 모았다.
매뉴얼, FPGA 소스, MCU 프로젝트가 이 폴더 하나에 들어 있다.

**먼저 볼 문서: [13_DE1SoC_Link 실습매뉴얼.html](13_DE1SoC_Link%20실습매뉴얼.html)** (배선, 단계별 절차, 확인 목록, 문제 해결)

| 번호 | 예제 | 계층 | 핵심 | 짝이 되는 FPGA 최상위 모듈 |
|---|---|---|---|---|
| 01 | [01_Link_UART_Frame_HAL_c](01_Link_UART_Frame_HAL_c) | HAL | 1초마다 `A` 한 바이트 송신, FPGA가 프레임을 LED 10개에 표시 | `fpga/lab1_top.v` |
| 02 | [02_Link_UART_Echo_HAL_c](02_Link_UART_Echo_HAL_c) | HAL | PC 터미널 ↔ MCU ↔ FPGA 중계, FPGA가 +1 해서 응답 | `fpga/lab2_top.v` |
| 03 | [03_Link_PWM_Meter_HAL_c](03_Link_PWM_Meter_HAL_c) | HAL | 1 kHz PWM 듀티 5단계, FPGA가 주기·High 시간 측정 | `fpga/lab3_top.v` |
| 04 | [04_Link_Clock_Counter_HAL_c](04_Link_Clock_Counter_HAL_c) | HAL | TIM1_CH2(PA9)에서 1 Hz 클럭 출력, FPGA가 상승 엣지를 세어 LEDR0~9(2진수)와 HEX0~5(16진수)에 표시 | `fpga/lab4_top.v` |

폴더명 끝의 `_c`는 사용자 검토 전이라는 표시이다.

## 폴더 구성

| 위치 | 내용 |
|---|---|
| `13_DE1SoC_Link 실습매뉴얼.html` | 실습 매뉴얼. 코드 복사 단추와 확인 목록이 있어 HTML 그대로 본다 |
| `img/` | 매뉴얼이 쓰는 그림 (UM1724 Figure 13) |
| `fpga/` | Verilog 소스 8개(`common.v`, `frame_leds.v`, `uart_rx.v`, `uart_tx.v`, `lab1_top.v`~`lab4_top.v`)와 핀 배치 스크립트 `pins.tcl` |
| `01_…` ~ `04_…` | MCU 프로젝트 (VS Code + PlatformIO, CubeMX `.ioc` 포함) |

## 공통 배선

| 신호 | Nucleo | DE1-SoC JP1 | 쓰는 실습 |
|---|---|---|---|
| UART MCU→FPGA / 클럭 | D8 (PA9, USART1_TX 또는 TIM1_CH2) | 1번 핀, GPIO_0[0], PIN_AC18 | 1, 2 (UART), 4 (클럭) |
| UART FPGA→MCU | D2 (PA10, USART1_RX) | 2번 핀, GPIO_0[1], PIN_Y17 | 2 |
| PWM | D7 (PA8, TIM1_CH1) | 3번 핀, GPIO_0[2], PIN_AD17 | 3 |
| 접지 | GND | 12번 핀, GND | 전체 |

두 보드 모두 3.3 V 입출력이다. JP1의 11번(5 V)과 29번(3.3 V)에는 아무것도 연결하지 않는다.
**실습 3을 시작하기 전에 D2 선을 뺀다. 실습 4는 D2를 쓰지 않는다.** 실습 3의 MCU 프로그램은 PA10(D2)에도 PWM 출력을 켜고, 실습 2의 FPGA 회로는 같은 선을 출력으로 구동한다.

## 개발 환경

- MCU: VS Code + PlatformIO (`framework = stm32cube`), 시스템 클럭 64 MHz(HSI/2 × 16)
- FPGA: Quartus Prime Lite 18.1, Cyclone V 5CSEMA5F31C6. Quartus 프로젝트는 각자 만들고 `fpga/`의 파일을 넣는다(매뉴얼 「Quartus 프로젝트 만들기」).

## 검증 상태

- Verilog는 Icarus Verilog 12.0 시뮬레이션으로 확인했다. `lab4_top.v`는 ModelSim ASE로 클럭 1030개를 넣어 카운터 0x406, LEDR = 6(1030 mod 1024), SW0 리셋 동작을 확인했다.
- MCU 프로젝트는 원본 예제(`02_USART/08_USART_printf_Dual`, `06_Timers_RTC/05_TIM_PWM`)의 `main.c`에서 `USER CODE` 영역만 고친 것이며, 빌드해 보지는 않았다.
- Quartus 컴파일과 실제 보드 동작은 확인하지 않았다. JP1 헤더의 핀 번호 위치는 매뉴얼 원문 그림으로 대조하지 못했으므로 배선 전에 실크 인쇄와 멀티미터로 확인한다.

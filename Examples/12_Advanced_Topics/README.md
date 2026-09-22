# 12_Advanced_Topics — 심화(선택) 예제

3학년 2학기 "임베디드시스템" 교과목의 기본 진도보다 앞서 나가는 예제를 모아 둔 그룹이다.
개념 자체(예: "타이머는 카운터다", "DMA는 CPU 없이 데이터를 옮긴다")는 다른 그룹에서 이미 익혔고,
여기서는 그 개념의 **실무·심화 응용**(RTOS 내부 구조, 통신 프로토콜 설계, 버스 중재, 보안 설정 등)을 다룬다.

**권장 학습 순서에는 포함하지 않는다.** 관심 있는 학생이 각 주제의 기본 예제를 먼저 끝낸 뒤 선택적으로 본다.

| 번호 | 예제 | 계층 | 핵심 | 먼저 볼 기본 예제 |
|---|---|---|---|---|
| 01 | [01_ADC_Injected_HAL_c](01_ADC_Injected_HAL_c/README.md) | HAL, DMA | 주입 채널이 연속 변환에 끼어들기 | 03_ADC/03_ADC_AnalogWatchdog_HAL_c (연속 변환 + DMA 패턴) |
| 02 | [02_ADC_DualMode_HAL_c](02_ADC_DualMode_HAL_c/README.md) | HAL, DMA | ADC1+ADC2 동시 샘플링(듀얼 모드), 32비트 DMA | 03_ADC/02_ADC_DMA_TimTrigger_HAL_c (단일 ADC 스캔) |
| 03 | [03_I2C_Slave_Loopback_HAL_c](03_I2C_Slave_Loopback_HAL_c/README.md) | HAL | 슬레이브 모드, 리슨/주소 일치 콜백 | 04_I2C_Communication/02_I2C_Scan_HAL_c |
| 04 | [04_SPI_Slave_Loopback_HAL_c](04_SPI_Slave_Loopback_HAL_c/README.md) | HAL | 슬레이브 모드, 하드웨어 NSS, 응답 장전 | 05_SPI_Communication/01_SPI_Loopback_HAL_c |
| 05 | [05_TIM_Cascade32_HAL_c](05_TIM_Cascade32_HAL_c/README.md) | HAL | 마스터/슬레이브로 잇는 32비트 카운터, 스톱워치 | 06_Timers_RTC/01_TIM_TimeBase |
| 06 | [06_DMA_Priority_HAL_c](06_DMA_Priority_HAL_c/README.md) | HAL | 채널 우선순위와 중재, 완료 시각 측정 | 07_DMA/01_DMA_MemToMem_HAL_c |
| 07 | [07_PendSV_ContextSwitch_Reg_c](07_PendSV_ContextSwitch_Reg_c/README.md) | 레지스터+ASM | 미니 선점형 RTOS | 08_Clock_System/03_ExceptionModel_VectorTable_Reg_c (예외 진입/복귀 기초) |
| 08 | [08_HSE_PLL_72MHz_Reg_c](08_HSE_PLL_72MHz_Reg_c/README.md) | 레지스터 | HSE+PLL 72MHz, 실패 시 HSI 복귀 | 08_Clock_System/04_Clock_Config_Reg_c (PLL 전환 기초) |
| 09 | [09_ITM_SWO_Printf_Reg_c](09_ITM_SWO_Printf_Reg_c/README.md) | 레지스터 | SWO 로 printf (UART 없이) | 02_USART/01_USART_printf (UART printf 로 이미 디버깅 가능) |
| 10 | [10_StackWatermark_Reg_c](10_StackWatermark_Reg_c/README.md) | 레지스터 | 스택 페인팅, 하이 워터마크, 재귀 실험 | 08_Clock_System/02_MemoryMap_Reg_c (스택이 SRAM 어디 있는지) |
| 11 | [11_IAP_Bootloader_Reg_c](11_IAP_Bootloader_Reg_c/README.md) | 레지스터 | 응용으로 점프, VTOR 교체, 링커 스크립트 | 10_Flash_CRC/01_Flash_Write_HAL_c (Flash 소거/쓰기 기초) |
| 12 | [12_IAP_App_Reg_c](12_IAP_App_Reg_c/README.md) | 레지스터 | 0x08008000 에서 도는 응용 프로그램 | 10_Flash_CRC/01_Flash_Write_HAL_c |
| 13 | [13_IAP_Xmodem_Bootloader_Reg_c](13_IAP_Xmodem_Bootloader_Reg_c/README.md) | 레지스터 | UART XMODEM-CRC 로 펌웨어 수신·Flash 기록·점프 | (11_IAP_Bootloader_Reg_c, 12_IAP_App_Reg_c 와 세트) |
| 14 | [14_OptionBytes_Read_HAL_c](14_OptionBytes_Read_HAL_c/README.md) | HAL | 옵션 바이트 읽기(RDP/WRP/USER), 읽기 전용 | 10_Flash_CRC/01_Flash_Write_HAL_c |
| 15 | [15_CAN_Normal_2boards_HAL_c](15_CAN_Normal_2boards_HAL_c/README.md) | HAL | 실제 버스 2보드, 오류 카운터, 버스오프 복구 | 11_CAN_Communication/01_CAN_Loopback_HAL_c |

## 묶음별로 보면

- **01, 02 ADC 심화**: 주입 채널, 듀얼 모드 동시 샘플링. ADC 개념(샘플링·분해능·DMA)을 먼저 익힌 뒤 볼 응용.
- **03, 04 슬레이브 모드**: I2C/SPI 를 "주변장치"가 아니라 "피주변장치"로 쓰는 법. 보드 한 장 안에서 마스터/슬레이브를 동시에 돌린다.
- **05 타이머 연결, 06 DMA 중재**: 두 타이머를 이어 32비트로 만들기, DMA 채널 간 우선순위 경쟁 — 응용 기교에 가깝다.
- **07~10 코어/디버그 심화**: PendSV 로 손수 구현한 미니 RTOS(RTOS 내부구조 과목 주제), HSE 72MHz, ITM/SWO, 스택 워터마크.
- **11~14 Flash/부트/보안**: IAP 부트로더 한 쌍, UART 로 새 펌웨어를 받는 XMODEM 부트로더, 옵션 바이트 읽기. 펌웨어 엔지니어 실무 주제.
- **15 CAN 실제 버스**: 트랜시버 2개 + 보드 2장, 버스오프 복구. 차량용 네트워크 심화 주제.

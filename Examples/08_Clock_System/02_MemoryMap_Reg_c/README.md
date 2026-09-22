# 02_MemoryMap_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

`GPIOA->ODR = ...`처럼 화살표로 쓰던 레지스터가 사실은 **그냥 메모리 주소**임을 보여준다. Cortex-M3 는 4GB 주소 공간을 Code/SRAM/Peripheral/PPB 로 미리 나눠 두고(memory-mapped I/O), SRAM 을 읽는 것과 똑같은 명령(LDR/STR)으로 주변장치를 읽고 쓴다.

## 핵심 학습 내용

- **4GB 주소 공간**(PM0056 2.2.1, RM0008 3.3): Code(0x0000_0000~), SRAM(0x2000_0000~), Peripheral(0x4000_0000~), PPB(0xE000_0000~, NVIC/SCB/SysTick 같은 코어 자체 레지스터).
- **버스 계층**(RM0008 3.1): AHB/APB2(고속)/APB1(저속)에 따라 GPIOA(0x4001...), TIM2(0x4000...), RCC(0x40021000)의 주소 블록이 나뉜다. CMSIS 포인터(`GPIOA`, `RCC`, `NVIC`...)가 이미 계산해 둔 값을 직접 출력해 확인.
- **memory-mapped I/O 증명**: 같은 `GPIOA->ODR` 을 "구조체 화살표"와 "리터럴 주소 역참조" 두 가지로 건드려 완전히 같은 메모리임을 보인다.
- **내 프로그램은 실제로 어디 있는가**: 문자열 리터럴(Flash=Code), 전역 변수(SRAM), 현재 SP(SRAM) 의 주소를 출력해 표와 대조.
- **리틀 엔디안**(2.2.6): 32비트 값을 바이트 단위로 읽어 낮은 주소에 하위 바이트가 오는 것을 확인.

## 배선 / 동작

배선 없음. UART(115200) 출력.

## ISR

사용하지 않는다.

## 관련 예제

`01_CoreRegisters_Instructions_Reg_c`, `01_GPIO/09_BitBanding_Reg_c`(메모리 매핑을 이용한 원자적 비트 접근, 세 번째 접근 방식).

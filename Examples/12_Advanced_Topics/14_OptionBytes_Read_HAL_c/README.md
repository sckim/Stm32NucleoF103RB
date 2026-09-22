# 14_OptionBytes_Read_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**옵션 바이트 읽기 전용** 조회: 읽기 보호(RDP), 쓰기 보호(WRP), USER 설정, 원시 옵션 바이트 메모리와 `FLASH->OBR/WRPR`. 옵션 바이트를 **바꾸지 않는다**.

## 핵심 학습 내용

- **옵션 바이트**(RM0008 3.3.3): 시스템 영역(`0x1FFFF800~`)의 칩 설정. 항목마다 값과 **보수**를 함께 저장하고 불일치하면 `OBR.OPTERR`.
- **RDP**: `0xA5` = 레벨 0(보호 없음), 그 외 = 레벨 1(디버거로 Flash 읽기 불가). **레벨 1 해제 시 Flash 전체가 자동 소거**된다.
- **USER**: `nWDG_SW`(0 이면 하드웨어 워치독 자동 시작), `nRST_STOP`, `nRST_STDBY`. **WRP**: 4페이지(4KB) 단위 쓰기 보호.
- `HAL_FLASHEx_OBGetConfig`로 구조체로 읽고, 원시 메모리를 함께 덤프해 값/보수 쌍을 눈으로 확인한다.

## 배선 / 동작

UART(115200)로 한 번 출력하고 대기. 변경은 STM32CubeProgrammer OB 탭 또는 `HAL_FLASHEx_OBProgram`(+`OB_Launch`) 으로 하며, RDP 는 건드리지 말 것.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`10_Flash_CRC/01_Flash_Write_HAL_c`, `11_IAP_Bootloader_Reg_c`(쓰기 보호와 부트로더), `09_WatchDog_Sleep/07_IWDG_HAL_c`(WDG_SW 옵션).

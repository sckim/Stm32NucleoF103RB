# 11_IAP_Bootloader_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

**IAP 부트로더**: Flash 앞 32KB(`0x08000000~0x08007FFF`)에 상주하며, 유효한 응용 프로그램(`0x08008000~`)이 있으면 벡터 테이블을 교체하고 그쪽으로 점프한다. 짝 예제 `12_IAP_App_Reg_c`와 함께 사용한다.

## 핵심 학습 내용

- **유효성 검사**: 응용의 첫 두 워드 = 초기 MSP(SRAM 범위 0x20000000~0x20005000), Reset 벡터(Flash 범위 + Thumb 비트 1). 빈 Flash(`0xFFFFFFFF`)에는 점프하지 않는다.
- **점프 순서**: 인터럽트 끄기 → SysTick 정지 → `NVIC->ICER/ICPR`로 모든 인터럽트 비활성/펜딩 해제 → `SCB->VTOR = 앱 주소` → `__set_MSP(앱 MSP)` → 앱 `Reset_Handler` 분기.
- **링커 스크립트**: `ldscript.ld`가 `FLASH LENGTH = 32K` 로 부트로더 영역을 제한 (`board_build.ldscript`로 지정).
- **부트로더 유지**: B1 을 누른 채 리셋하면 점프하지 않고 남는다(펌웨어 업데이트 대기 자리). 유효한 앱이 없으면 LD2 느린 점멸.

## 배선 / 동작

1) 이 프로젝트를 업로드 → 2) `12_IAP_App_Reg_c`를 업로드(ELF 주소 0x08008000 부터만 기록하므로 부트로더는 안 지워진다. 칩 전체 삭제 옵션은 쓰지 말 것) → 3) 리셋하면 배너 후 앱이 실행된다. UART(115200) 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`12_IAP_App_Reg_c`(짝), `10_Flash_CRC/01_Flash_Write_HAL_c`, Docs `AN2606`(ST 시스템 메모리 부트로더).

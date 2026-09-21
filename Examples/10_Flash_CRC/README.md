# 10_Flash_CRC — 내부 Flash와 CRC

내부 Flash를 데이터 저장소로 쓰는 방법과, 데이터 무결성을 검사하는 하드웨어 CRC 유닛을 다룬다. 두 기능은 "저장한 데이터가 유효한가"라는 문제로 이어진다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_Flash_Write_HAL_c](01_Flash_Write_HAL_c/README.md) | HAL | 페이지 소거/반워드 쓰기, 웨어 레벨링 |
| 02 | [02_CRC_Unit_HAL_c](02_CRC_Unit_HAL_c/README.md) | HAL | 하드웨어 CRC-32/MPEG-2, 소프트웨어와 비교 |
| 03 | [03_IAP_Bootloader_Reg_c](03_IAP_Bootloader_Reg_c/README.md) | 레지스터 | 응용으로 점프, VTOR 교체, 링커 스크립트 |
| 04 | [04_IAP_App_Reg_c](04_IAP_App_Reg_c/README.md) | 레지스터 | 0x08008000 에서 도는 응용 프로그램 |
| 05 | [05_OptionBytes_Read_HAL_c](05_OptionBytes_Read_HAL_c/README.md) | HAL | 옵션 바이트 읽기(RDP/WRP/USER), 읽기 전용 |
| 06 | [06_IAP_Xmodem_Bootloader_Reg_c](06_IAP_Xmodem_Bootloader_Reg_c/README.md) | 레지스터 | UART XMODEM-CRC 로 펌웨어 수신·Flash 기록·점프 |

## 이 그룹에서 배우는 것

- Flash 특성: "지우기(1로 채움) → 필요한 비트만 0으로 쓰기", **소거는 페이지(1KB) 단위**, 수명 약 10,000회
- 값이 바뀔 때마다 지우지 않고 빈 칸에 덧붙이는 **웨어 레벨링** 기법
- 쓰기/소거 중 Flash 접근이 막혀 코드 실행이 멈추는 문제
- CRC로 오류 검출: 부트로더의 펌웨어 검증, 통신 프레임 검사, 저장 데이터 유효성 표시

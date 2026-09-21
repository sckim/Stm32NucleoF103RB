# 05_SPI_SDcard_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

**SPI 모드 SD 카드**의 초기화와 섹터 읽기를 파일시스템 없이 구현한다. 카드 종류(SDSC/SDHC), 용량, 섹터 0 의 MBR 서명을 UART 로 출력한다. **읽기만 하며 카드 내용을 바꾸지 않는다.**

## 핵심 학습 내용

- **초기화 순서**: 74클럭 이상(400kHz 이하) → `CMD0`(SPI 모드, CRC 0x95) → `CMD8`(전압 확인, CRC 0x87) → `ACMD41`(HCS) 반복 → `CMD58`(OCR, CCS 비트로 SDHC 판별) → 클럭을 8MHz 로.
- **블록 읽기**: `CMD17` → R1 → 데이터 토큰 `0xFE` → 512바이트 → CRC 2바이트. SDHC 는 섹터 번호, SDSC 는 바이트 주소.
- **용량**: `CMD9`로 CSD 를 읽어 `C_SIZE`에서 계산 (CSD v2: (C_SIZE+1) x 512KB).
- **다음 단계**: FatFs(프레임워크의 `Middlewares/Third_Party/FatFs`)를 이 읽기/쓰기 블록 함수 위에 붙이면 파일 접근이 된다.

## 배선 / 동작

마이크로 SD 모듈(3.3V 전용): SCK=PA5, MOSI=PA7, MISO=PA6(10kΩ 풀업 권장), CS=PA4. PA5 는 LD2 와 같은 핀이다. UART(115200)로 결과 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다(블로킹 SPI). v2 이상 카드만 지원한다(구형 v1 카드/MMC 는 미지원).

## 관련 예제

`01_SPI_Loopback_HAL_c`, `10_Flash_CRC/02_CRC_Unit_HAL_c`(데이터 무결성 검사).

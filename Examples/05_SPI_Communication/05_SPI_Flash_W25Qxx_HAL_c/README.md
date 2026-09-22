# 05_SPI_Flash_W25Qxx_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**SPI NOR Flash(W25Qxx)** 제어: JEDEC ID, 4KB 섹터 소거, 256바이트 페이지 프로그램, 읽기 검증. 내부 Flash(`01_Flash_Write`)와 같은 "1→0 으로만 쓰기, 소거는 섹터 단위" 규칙을 SPI 명령으로 경험한다.

## 핵심 학습 내용

- **명령**: `0x9F` JEDEC ID, `0x05` 상태(BUSY/WEL), `0x06` Write Enable, `0x20` 섹터 소거, `0x02` 페이지 프로그램, `0x03` 읽기, `0x4B` 고유 ID.
- **WEL 래치**: 모든 쓰기/소거 전에 Write Enable 이 필요하고 명령 완료 후 자동으로 꺼진다. 완료는 상태 레지스터 **BUSY 비트 폴링**으로 확인.
- **페이지 경계**: 프로그램은 256바이트 페이지 안에서만 (넘으면 되감김).
- 테스트 섹터(0x001000)를 소거 → 전부 0xFF 확인 → 256바이트 패턴 프로그램 → 읽어서 PASS/FAIL. 소요 시간 출력. **그 4KB 의 내용은 지워진다.**

## 배선 / 동작

W25Qxx 모듈(3.3V): CLK=PA5, DI=PA7, DO=PA6, CS=PA4, /WP,/HOLD 는 3.3V. PA5 는 LD2 와 같은 핀. UART(115200) 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다 (블로킹 SPI).

## 관련 예제

`10_Flash_CRC/01_Flash_Write_HAL_c`(내부 Flash), `04_SPI_SDcard_HAL_c`(다른 SPI 저장 장치).

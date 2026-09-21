# 05_SPI_Communication — SPI

SPI1(SCK = PA5, MISO = PA6, MOSI = PA7) 마스터 전이중 통신을 폴링과 DMA로 다룬다. 슬레이브 장치 없이 **MOSI-MISO 점퍼(루프백)** 만으로 시험한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_SPI_Loopback_HAL_c](01_SPI_Loopback_HAL_c/README.md) | HAL | 루프백 검증, 분주비별 속도 측정 |
| 02 | [02_SPI_DMA_Fullduplex_HAL_c](02_SPI_DMA_Fullduplex_HAL_c/README.md) | HAL, DMA | DMA 전이중, 폴링과 CPU 여유 비교 |
| 03 | [03_SPI_Slave_Loopback_HAL_c](03_SPI_Slave_Loopback_HAL_c/README.md) | HAL | 슬레이브 모드, 하드웨어 NSS, 응답 장전 |
| 04 | [04_SPI_OLED_SSD1306_HAL_c](04_SPI_OLED_SSD1306_HAL_c/README.md) | HAL | D/C 핀 방식 OLED, 8MHz 고속 갱신 |
| 05 | [05_SPI_SDcard_HAL_c](05_SPI_SDcard_HAL_c/README.md) | HAL | SD 카드 SPI 초기화, 섹터 읽기(MBR) |
| 06 | [06_SPI_Flash_W25Qxx_HAL_c](06_SPI_Flash_W25Qxx_HAL_c/README.md) | HAL | SPI NOR Flash: ID, 섹터 소거, 페이지 프로그램 |
| 07 | [07_SPI_MAX7219_HAL_c](07_SPI_MAX7219_HAL_c/README.md) | HAL | 8x8 LED 매트릭스, 16비트 프레임/LOAD 반영 |

## 이 그룹에서 배우는 것

- SPI 4선(SCK/MOSI/MISO/CS), 모드 0~3(CPOL/CPHA), 전이중(송신과 수신이 같은 클럭에 동시 진행)
- 분주비와 실제 비트율의 관계 (APB2 64MHz / 분주)
- 큰 데이터 전송에서 **DMA가 필요한 이유** (OLED/TFT/SD 카드)

## 공통 배선

`PA7 (MOSI, D11) ── 점퍼 ── PA6 (MISO, D12)`. SCK = PA5(D13)는 온보드 LD2와 같은 핀이라 전송 중 LED가 흐릿하게 켜진다(정상). 점퍼를 빼면 수신이 0x00/0xFF만 나와 FAIL이 되는 음성 대조 시험도 가능하다.

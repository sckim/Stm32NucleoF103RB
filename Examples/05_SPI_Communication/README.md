# 05_SPI_Communication — SPI

SPI1(SCK = PA5, MISO = PA6, MOSI = PA7) 마스터 전이중 통신을 폴링과 DMA로 다룬다. 슬레이브 장치 없이 **MOSI-MISO 점퍼(루프백)** 만으로 시험한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 40 | [40_SPI_Loopback_HAL_c](40_SPI_Loopback_HAL_c/README.md) | HAL | 루프백 검증, 분주비별 속도 측정 |
| 41 | [41_SPI_DMA_Fullduplex_HAL_c](41_SPI_DMA_Fullduplex_HAL_c/README.md) | HAL, DMA | DMA 전이중, 폴링과 CPU 여유 비교 |

## 이 그룹에서 배우는 것

- SPI 4선(SCK/MOSI/MISO/CS), 모드 0~3(CPOL/CPHA), 전이중(송신과 수신이 같은 클럭에 동시 진행)
- 분주비와 실제 비트율의 관계 (APB2 64MHz / 분주)
- 큰 데이터 전송에서 **DMA가 필요한 이유** (OLED/TFT/SD 카드)

## 공통 배선

`PA7 (MOSI, D11) ── 점퍼 ── PA6 (MISO, D12)`. SCK = PA5(D13)는 온보드 LD2와 같은 핀이라 전송 중 LED가 흐릿하게 켜진다(정상). 점퍼를 빼면 수신이 0x00/0xFF만 나와 FAIL이 되는 음성 대조 시험도 가능하다.

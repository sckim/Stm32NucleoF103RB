# 07_DMA — DMA

DMA(Direct Memory Access)는 CPU 개입 없이 버스 마스터로 데이터를 옮기는 장치이다. STM32F103RB는 DMA1(7채널)을 가진다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_DMA_MemToMem](01_DMA_MemToMem) | HAL | 메모리→메모리 복사, CPU 복사와 사이클 비교 |
| 02 | [02_DMA_Circular_GPIO](02_DMA_Circular_GPIO) | HAL, TIM, DMA | 원형 DMA + 핑퐁 버퍼로 GPIO 패턴 출력 |
| 03 | [03_DMA_Width_Error_Reg_c](03_DMA_Width_Error_Reg_c) | 레지스터 | 데이터 폭 변환 9조합, APB 복제, EN=1 규칙, 전송 오류(TEIF), CPU와의 버스 공유 실측 |

03은 강의자료 교안 「DMA」 장에 맞춰 추가한 레지스터 수준 보충 예제이다.

**개념설명**: [07_DMA 개념설명.html](07_DMA%20개념설명.html)은 DMA 제어기를 RM0008의 원본 블록도로 설명한다. 다루는 내용은 버스 마스터와 버스 행렬, 채널의 디지털논리 등가 모델, 요청 매핑과 중재, 폭 변환, HT·TC 핑퐁 시간 계산, 플래그·인터럽트, 설계상 한계와 정오표, 확인 문제이며, 각 예제가 어느 경로를 쓰는지 연결한다. 그림 원본은 `img/` 폴더에 있다.

예제설명(HTML): 각 예제 폴더의 `<예제명> 예제설명.html`(예: `01_DMA_MemToMem/01_DMA_MemToMem 예제설명.html`).

## 이 그룹에서 배우는 것

- DMA 채널 구성 요소: 소스/목적지 주소, 전송 개수(`CNDTR`), 주소 증가(PINC/MINC), 데이터 폭, 방향, 원형 모드
- 완료 통지: 전송 완료(TC)/반 완료(HT) 인터럽트와 콜백
- DMA가 실제로 쓰이는 곳: 주변장치 ↔ 메모리 — `02_USART/04_USART_DMA_IdleLine_c`(UART 수신), `03_ADC/04_ADC_DMA_TimTrigger`(ADC), `05_SPI/02_SPI_DMA_Fullduplex_c`(SPI)

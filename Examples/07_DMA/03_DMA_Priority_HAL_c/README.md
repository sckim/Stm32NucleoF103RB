# 03_DMA_Priority_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (컴파일만 확인)

**DMA 채널 우선순위와 중재(arbiter)**: 채널1·채널2 가 각각 4KB 메모리 복사를 동시에 시작할 때 어느 쪽이 먼저 끝나는지 DWT 사이클로 측정한다. B1 버튼으로 세 가지 설정을 순환한다.

## 핵심 학습 내용

- **중재 규칙**(RM0008 13.3.2): 소프트웨어 우선순위 `CCR.PL`(Low < Medium < High < Very high), 같으면 **채널 번호가 낮은 쪽**이 먼저. 중재는 전송 1개 단위.
- **케이스 A/B/C**: (ch1=Low, ch2=Very high) / (ch1=Very high, ch2=Low) / (둘 다 Medium). 우선순위가 높은 채널이 먼저 끝나고 낮은 채널은 그 뒤에 이어서 진행.
- **측정**: 두 채널을 인터럽트 금지 상태에서 연달아 시작하고 완료 콜백에서 `DWT->CYCCNT` 차이를 기록.
- 실제 설계 교훈: 지연에 민감한 스트림(오디오, ADC 샘플링)에 높은 우선순위를 준다.

## 배선 / 동작

배선 없음. B1 을 누를 때마다 케이스가 바뀌고, 각 케이스를 3회 측정해 UART(115200)에 완료 시각/먼저 끝난 채널/복사 검증(OK/FAIL)을 출력.

## ISR vs 콜백

`DMA1_Channel1/2_IRQHandler`(진짜 ISR) → `HAL_DMA_IRQHandler()` → `done1_cb`/`done2_cb`(이 파일).

## 관련 예제

`01_DMA_MemToMem_HAL_c`, `02_DMA_Circular_GPIO_HAL_c`, `08_Clock_System/08_DWT_Profiling_Reg_c`.

# 10_DWT_Profiling_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

DWT의 **사이클 카운터(CYCCNT)** 로 코드 실행 시간을 사이클 단위로 측정한다.

## 핵심 학습 내용

- **DWT `CYCCNT`**: 코어 클럭마다 1씩 증가하는 32비트 카운터 (Cortex-M3/M4/M7, M0에는 없음). 타이머 자원 없이 함수 하나의 비용을 정확히 잴 수 있어 최적화 전후 비교에 유용하다.
- **활성화**: `CoreDebug->DEMCR.TRCENA(bit24)=1` → `DWT->CYCCNT = 0` → `DWT->CTRL.CYCCNTENA(bit0)=1`.
- **래핑**: 32비트라 8MHz에서 약 536초, 64MHz에서 약 67초 후 래핑. `끝 - 시작`을 부호 없는 뺄셈으로 계산하면 1회 래핑에 안전.
- **측정 오버헤드 제거**: 빈 함수 호출(`baseline`)을 측정해 이후 값에서 뺀다.
- **측정 항목** (HSI 8MHz, Flash 0 대기, `-Os`): 정수 덧셈/곱셈/나눗셈(SDIV), **64비트 나눗셈**(하드웨어 없음 → 라이브러리 루틴), **소프트웨어 부동소수점** 곱/나눗셈(FPU 없음), `memcpy` 256B, GPIO 토글(ODR 읽기-수정-쓰기). Cortex-M3에 FPU가 없다는 점의 체감.
- `volatile`로 컴파일러의 상수 접기/제거를 막는다. 값은 최적화 수준과 Flash 대기에 따라 달라진다.

## 동작

UART(115200)로 "사이클 수 / 8MHz 기준 시간" 표를 출력.

## 인터럽트

사용하지 않는다.

## 관련 예제

`05_SPI_Communication/01_SPI_Loopback_HAL_c`, `07_DMA/01_DMA_MemToMem_HAL_c`, `10_Flash_CRC/02_CRC_Unit_HAL_c`(DWT로 시간 측정 활용), `06_NVIC_Priority_HAL_c`.

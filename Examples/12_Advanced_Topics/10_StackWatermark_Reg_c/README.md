# 10_StackWatermark_Reg_c

> 계층: **레지스터(CMSIS) + 링커 심볼** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**스택 사용량 측정**(스택 페인팅과 하이 워터마크). F103 은 MPU 가 없어 스택이 힙/데이터를 덮어써도 폴트가 나지 않으므로, 얼마나 썼는지 직접 재는 습관이 필요하다.

## 핵심 학습 내용

- **페인팅**: 부팅 직후 미사용 스택 영역을 `0xDEADBEEF` 로 채우고, 나중에 낮은 주소부터 패턴이 남은 곳까지 세면 = 한 번도 안 쓴 여유. 최대 사용량 = 전체 − 여유.
- **링커 심볼**: `end`(.bss 끝 = 힙 시작), `_estack`(RAM 끝), `_Min_Stack_Size`(0x400).  `__get_MSP()` 로 현재 SP.
- **재귀 실험**(B1): 누를 때마다 재귀 깊이를 12프레임(약 1.2KB) 늘리고 워터마크/여유를 출력. 여유가 128바이트 미만이면 더 늘리지 않는다(실제 오버플로는 일으키지 않음).
- 실전: 최악 경로(중첩 인터럽트 + 가장 깊은 호출)를 시험한 뒤 스택에 20~50% 여유를 준다. RTOS 의 `uxTaskGetStackHighWaterMark` 와 같은 원리.

## 배선 / 동작

B1 을 누를 때마다 UART(115200)에 `depth / max stack used / free` 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`08_Clock_System/08_HardFault_Diagnosis_Reg_c`, `07_PendSV_ContextSwitch_Reg_c`(작업별 스택), `08_Clock_System/10_DWT_Profiling_Reg_c`.

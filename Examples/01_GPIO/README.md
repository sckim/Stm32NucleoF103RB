# 01_GPIO — 출력, 입력, 외부 인터럽트

LED 점멸부터 시작해 버튼 입력(폴링/인터럽트)까지 GPIO의 기본을 다룬다. 앞의 네 예제(`01_BlinkArduino`~`04_BlinkReg`)는 **똑같은 Blink를 서로 다른 추상화 계층으로** 구현해 계층별 차이를 비교하게 한다.

| 번호 | 예제 | 계층 | 핵심 |
|---|---|---|---|
| 01 | [01_BlinkArduino](01_BlinkArduino/README.md) | Arduino | `pinMode`/`digitalWrite`/`delay` |
| 02 | [02_BlinkHAL](02_BlinkHAL/README.md) | HAL | `HAL_GPIO_TogglePin`, CubeMX 프로젝트 구조 |
| 03 | [03_BlinkLL](03_BlinkLL/README.md) | LL | `LL_GPIO_TogglePin`, JTAG 해제 코드 |
| 04 | [04_BlinkReg](04_BlinkReg/README.md) | 레지스터 | `RCC`/`GPIOx->CRL`/`BSRR`, SysTick 딜레이 |
| 05 | [05_ReadPin](05_ReadPin/README.md) | HAL | 버튼 폴링 입력 |
| 06 | [06_ExtInt](06_ExtInt/README.md) | HAL, EXTI | 버튼 인터럽트와 콜백 |
| 07 | [07_ReadPinReg](07_ReadPinReg/README.md) | 레지스터 | `GPIOC->IDR` 직접 읽기 |
| 08 | [08_ExternalInt](08_ExternalInt/README.md) | HAL, EXTI | 두 EXTI 라인(`EXTI9_5`, `EXTI15_10`)과 공용 콜백 |
| 09 | [09_BitBanding_Reg_c](09_BitBanding_Reg_c/README.md) | 레지스터 | Cortex-M3 비트 밴딩 (검토 전) |

## 이 그룹에서 배우는 것

- GPIO 사용 3단계: **클럭 허용 → 모드 설정 → 읽기/쓰기**
- 같은 동작의 추상화 비용: 코드량·이식성·하드웨어 가시성의 트레이드오프
- 폴링 vs 인터럽트: 버튼 입력을 CPU가 계속 확인할지, 하드웨어가 알려 줄지
- `BSRR`로 원자적 비트 조작, `IDR`로 입력 읽기

## 공통 하드웨어

LD2 = PA5, B1 = PC13(눌림 = Low). 별도 배선 없이 보드만으로 실행된다(`08_ExternalInt`은 예외).

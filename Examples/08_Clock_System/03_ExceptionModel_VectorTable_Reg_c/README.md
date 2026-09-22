# 03_ExceptionModel_VectorTable_Reg_c

> 계층: **레지스터(CMSIS) + 어셈블리** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

인터럽트를 "HAL이 알아서 처리해 주는 것"이 아니라 하드웨어 수준에서 본다. `06_NVIC_Priority_HAL_c`(우선순위·선점)와 `08_HardFault_Diagnosis_Reg_c`(폴트 스택 해석)의 개념적 준비 단계.

## 핵심 학습 내용

- **벡터 테이블**(PM0056 2.3.4, Figure 12): 주소 0 은 "함수 주소들의 배열". 슬롯0=초기 SP, 슬롯1=Reset, …, 슬롯11=SVCall, 슬롯14=PendSV, 슬롯15=SysTick, 슬롯16~=주변장치 IRQ. CPU 는 이 배열에서 주소를 읽어 분기할 뿐이다.
- **예외 진입 시 자동 스택 저장**(2.3.7): 인터럽트가 나면 하드웨어가 소프트웨어 개입 없이 R0-R3,R12,LR,PC,xPSR 8워드를 스택에 쌓는다. SysTick 첫 진입에서 이 8워드를 그대로 캡처해 보여준다 — `08_HardFault_Diagnosis`가 다루는 "폴트 스택 프레임"과 같은 구조다.
- **EXC_RETURN**(2.3.7, Table 17): 예외 진입 시 LR 에 0xFFFFFFF9 같은 특수 값이 들어간다. `BX LR`이 그 값을 PC 에 쓰면 그 자체가 "복귀하라"는 신호. SVC(`svc 0`)를 직접 발생시켜 값을 확인한다.
- **우선순위는 레지스터의 숫자일 뿐**: `NVIC_GetPriority()`로 SysTick 우선순위를 읽어 확인 (실제 선점 실험은 `06_NVIC_Priority_HAL_c`).

## 배선 / 동작

배선 없음. UART(115200)로 벡터 테이블 덤프, 캡처한 스택 프레임, SVC 전후 결과를 출력.

## ISR

`SysTick_Handler`, `SVC_Handler` (이 파일에 직접 정의, naked 트램폴린 + 일반 C 함수 패턴).

## 관련 예제

`01_CoreRegisters_Instructions_Reg_c`, `08_HardFault_Diagnosis_Reg_c`(같은 스택 프레임을 폴트 상황에서 해석), `12_Advanced_Topics/07_PendSV_ContextSwitch_Reg_c`(수동으로 만든 프레임으로 문맥 전환).

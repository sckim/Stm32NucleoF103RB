# 04_IAP_App_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

**IAP 응용 프로그램**: `0x08008000`에서 시작하는 앱. `03_IAP_Bootloader_Reg_c`가 점프해 들어온다. 링크 주소·벡터 테이블 재배치·인터럽트 허용, 세 가지가 필요하다.

## 핵심 학습 내용

- **링크 위치**: `ldscript.ld`의 `FLASH ORIGIN = 0x08008000, LENGTH = 96K` → 벡터 테이블과 코드가 이 주소부터 배치된다.
- **벡터 테이블 오프셋**: `SCB->VTOR = 0x08008000`(PM0056 4.4.4). 부트로더가 설정하지만 앱도 시작 때 설정한다 (SysTick 이 앱의 핸들러로 가야 하므로).
- **인터럽트 허용**: 부트로더가 `__disable_irq()` 한 채 넘어오므로 앱이 `__enable_irq()`.
- **검증 포인트**: SysTick 인터럽트(1ms)로 LD2 가 200ms 주기로 깜빡이면 벡터 테이블 재배치가 성공한 것이다. UART 에 `VTOR=0x08008000`이 출력된다.

## 배선 / 동작

부트로더 없이 이 앱만 올리면 리셋 벡터가 `0x08000000`을 가리키므로 자동 실행되지 않는다(반드시 `03_IAP_Bootloader_Reg_c`가 먼저 필요).

## ISR vs 콜백

`SysTick_Handler`(진짜 ISR)만 사용. 콜백 개념 없음.

## 관련 예제

`03_IAP_Bootloader_Reg_c`(짝), `08_Clock_System/05_HardFault_Diagnosis_Reg_c`(벡터 테이블/스택 이해).

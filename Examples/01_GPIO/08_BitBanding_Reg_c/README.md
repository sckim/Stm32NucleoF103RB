# 08_BitBanding_Reg_c

> 계층: **레지스터(CMSIS)** — Claude 작성, 검토 전 (컴파일만 확인)

Cortex-M3의 **비트 밴딩(bit-banding)** 으로 레지스터/SRAM의 비트 하나를 원자적으로 읽고 쓴다.

## 핵심 학습 내용

- **비트 밴딩 원리**: 주변장치(0x40000000~) 또는 SRAM(0x20000000~) 영역의 각 비트가 별칭(alias) 영역(0x42000000~ / 0x22000000~)의 32비트 워드 하나에 1:1로 대응한다. 별칭 워드에 0/1을 쓰면 하드웨어가 해당 비트만 원자적으로 바꾼다.
  `alias = alias_base + (byte_offset × 32) + (bit_number × 4)`
- **왜 유용한가**: `reg |= (1<<n)`은 읽기→OR→쓰기 3단계라 그 사이 ISR이 같은 변수를 바꾸면 갱신이 사라진다. 별칭 쓰기는 한 번이므로 인터럽트를 막지 않고도 안전하다. (GPIO는 `BSRR`이 같은 목적을 이미 제공. 비트 밴딩은 `RCC->APB2ENR`, 타이머 `CR1.CEN`, SRAM 플래그 등에 유용)
- **Cortex-M3/M4 전용 기능**: M0/M0+ 에는 없다.

## 동작

1. `RCC->APB2ENR`의 `IOPAEN`(bit2), `IOPCEN`(bit4)을 비트 밴딩으로 켠다.
2. LD2(PA5)를 `GPIOA->ODR` bit5 별칭으로 토글, B1(PC13)은 `GPIOC->IDR` bit13 별칭으로 읽는다.
3. SRAM 플래그 워드의 bit0은 main, bit1은 SysTick ISR이 각자 비트 밴딩으로 세우고 지운다 → 경합 없음을 보인다.
4. B1을 누르고 있으면 LED가 빠르게, 아니면 느리게 깜빡인다.

## ISR

`SysTick_Handler`. 콜백 개념은 없다.

## 관련 예제

`03_BlinkReg`(일반 레지스터 조작 방식), `08_Clock_System/71_SysTick_Reg_c`.

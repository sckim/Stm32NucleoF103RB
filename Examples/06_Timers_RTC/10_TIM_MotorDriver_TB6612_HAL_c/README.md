# 10_TIM_MotorDriver_TB6612_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**H-브리지 모터 드라이버(TB6612FNG)** 로 DC 모터 제어: 20kHz PWM 속도, 방향 핀 2개, **쇼트 브레이크 vs 코스트**, STBY, B1 비상 정지.

## 핵심 학습 내용

- **AIN1/AIN2**: H/L 정회전, L/H 역회전, H/H 쇼트 브레이크(모터 단자 단락 → 빠른 정지), L/L 코스트(Hi-Z → 서서히 정지).
- **PWM 20kHz**: 가청 대역 위로 올려 모터 소리를 피한다. `CCR1 = 0..3200`(TIM2, 64MHz/3200).
- **시퀀스**: 정회전 가속 → 쇼트 브레이크 → 역회전 가속 → 코스트. 방향 전환 전에는 반드시 속도를 0 으로/브레이크를 거친다.
- **비상 정지**(B1): 브레이크 후 STBY Low. 다시 누르면 재개.

## 배선 / 동작

PWMA=PA0, AIN1=PB0, AIN2=PB1, STBY=PB2. 모터 전원(VM)은 **외부 전원**, GND 공통. 단계를 UART(115200)로 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`09_TIM_Servo_HAL_c`, `02_TIM_PWM_HAL_c`, `04_TIM1_DeadTime_Break_HAL_c`(브리지의 데드타임).

# 10_TIM_Cascade32_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

타이머 **마스터/슬레이브 연결**로 만든 32비트 카운터(스톱워치 약 71분). TIM3 의 넘침(TRGO)을 TIM2 의 외부 클럭(ITR2)으로 써서 16비트 두 개를 잇는다.

## 핵심 학습 내용

- **마스터 TIM3**: 1MHz, ARR=0xFFFF, 넘칠 때 `CR2.MMS=010`(update)로 TRGO 출력. **슬레이브 TIM2**: `SMCR.SMS=111`(외부 클럭 모드 1), `TS=010`(ITR2 = TIM3) (RM0008 15.3.15).
- **32비트 값** = `(TIM2->CNT << 16) | TIM3->CNT`, 1us 분해능으로 최대 2³² us ≈ 71.6분.
- **읽기 경합**: 상위-하위-상위 순으로 읽어 상위가 바뀌었으면 재시도.
- 1초마다 32비트 시각과 `HAL_GetTick`을 비교 출력하고, B1 로 스톱워치(시작/정지, h:m:s.ms)를 잰다.

## 배선 / 동작

배선 없음. UART(115200)로 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다.

## 관련 예제

`08_TIM_OnePulse_HCSR04_HAL_c`, `03_ADC/02_ADC_DMA_TimTrigger_HAL_c`(타이머 TRGO 를 다른 장치에 연결).

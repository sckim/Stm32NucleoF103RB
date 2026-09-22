# 07_IWDG_HAL_c

> 계층: **HAL** — Claude 작성, 검토 전 (PlatformIO 빌드만 확인)

**독립 워치독(IWDG)**: 저속 내부 클럭 LSI(약 40kHz)로 돌아 메인 클럭이 죽어도 살아 있는 "마지막 안전장치". 한 번 켜면 리셋 외에는 끌 수 없다.

## 핵심 학습 내용

- **WWDG 와의 차이**(`02_WWDG_HAL_c`): 창(window) 없이 타임아웃 전에만 리프레시하면 되고, 초 단위 타임아웃 가능, 독립 클럭.
- **타임아웃 계산**(RM0008 19장): `t = (reload+1) × prescaler / f_LSI`. 64분주, reload 624 → 공칭 1.0s. **LSI 는 30~60kHz 로 편차가 커서** 실제 0.67~1.33s. 리프레시 주기는 타임아웃의 절반 이하(0.5s).
- **IWDG->KR 키**: `0xCCCC` 시작, `0x5555` 설정 접근, `0xAAAA` 리프레시. 디버그 중 정지: `__HAL_DBGMCU_FREEZE_IWDG()`.
- **리셋 원인**: `RCC->CSR.IWDGRSTF`를 부팅 때 읽어 "이전 실행이 워치독 리셋이었음"을 알린다.

## 배선 / 동작

B1 을 누르면 프로그램이 멈춘 것을 흉내(리프레시 중단) → 약 1초 뒤 리셋 → 부팅 메시지에 IWDG 리셋 표시. UART(115200) 출력.

## ISR vs 콜백

인터럽트를 사용하지 않는다 (IWDG 는 리셋만 한다).

## 관련 예제

`02_WWDG_HAL_c`, `06_BKP_Tamper_HAL_c`, `08_Clock_System/12_DeviceInfo_UniqueID_Reg_c`(리셋 원인 출력).

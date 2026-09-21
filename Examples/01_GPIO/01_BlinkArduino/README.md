# 01_BlinkArduino

> 계층: **Arduino 프레임워크** (PlatformIO `framework = arduino`)

온보드 LED(LD2, PA5)를 0.5초 간격으로 점멸한다. 가장 높은 추상화 수준의 Blink이다.

## 핵심 학습 내용

- `setup()` 한 번 + `loop()` 반복 구조와 `pinMode` / `digitalWrite` / `delay` API
- Arduino 핀 맵과 Nucleo 핀의 대응: `LED_BUILTIN` = D13 = **PA5**
- 이 세 함수 뒤에서 일어나는 일(GPIOA 클럭 활성화, `CRL` 설정, `BSRR` 쓰기, SysTick 기반 `delay`)을 프레임워크가 대신 처리한다는 점. 이후 `02_BlinkHAL`~`04_BlinkReg`에서 이 숨겨진 부분을 계층별로 벗겨 본다.

## 동작

`digitalWrite(HIGH)` → `delay(500)` → `digitalWrite(LOW)` → `delay(500)` 반복.

## 실행

`platformio.ini`의 `monitor_port`(COM5)는 작성자 PC 기준이므로 환경에 맞게 수정한다. 이 예제는 시리얼 출력을 사용하지 않는다.

## 관련 예제

`02_BlinkHAL`(HAL), `03_BlinkLL`(LL), `04_BlinkReg`(레지스터)와 비교한다.

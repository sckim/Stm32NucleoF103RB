/**
 * 01_BlinkArduino  —  [계층: Arduino 프레임워크]  PlatformIO framework = arduino
 *
 * 같은 "LD2(PA5) 점멸"을 4가지 계층으로 구현하는 시리즈의 첫 번째(가장 높은 추상화)이다.
 *   01_BlinkArduino (Arduino) -> 02_BlinkHAL (HAL) -> 03_BlinkLL (LL) -> 04_BlinkReg (레지스터)
 *
 * pinMode()/digitalWrite()/delay() 한 줄이 내부에서는 다음 레지스터 조작으로 바뀐다.
 *   pinMode(OUTPUT)  : RCC_APB2ENR.IOPAEN = 1, GPIOA_CRL 의 CNF5/MODE5 설정
 *   digitalWrite()   : GPIOA_BSRR (BS5 = 켜기 / BR5 = 끄기)
 *   delay()          : SysTick 1ms 인터럽트가 증가시키는 카운터 기반
 * 레지스터를 직접 보고 싶다면 04_BlinkReg 를 비교해 보자.
 *
 * 이 파일에는 ISR/콜백이 없다 (SysTick 은 프레임워크 내부에서 처리).
 */
#include <Arduino.h>
// Nucleo-F103RB 보드의 온보드 녹색 LED는 PA5 핀에 연결되어 있음
// 아두이노 핀 맵 기준으로는 D13 핀에 해당함
const int ledPin = LED_BUILTIN; // 또는 13 또는 PA5 로 직접 지정 가능

void setup() {
    // 1. PA5 핀(D13)을 출력(OUTPUT) 모드로 설정
    // 내부적으로 GPIOA 클록 활성화 및 CRL/CRH 레지스터 제어가 자동으로 수행됨
    pinMode(ledPin, OUTPUT);
}

void loop() {
    // 2. 핀 출력을 HIGH(3.3V)로 인가하여 LED 켜기
    digitalWrite(ledPin, HIGH);

    // 3. SysTick 타이머 기반의 정밀 500ms 딜레이 가동
    delay(500);

    // 4. 핀 출력을 LOW(0V)로 인가하여 LED 끄기
    digitalWrite(ledPin, LOW);

    delay(500);
}
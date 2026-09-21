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
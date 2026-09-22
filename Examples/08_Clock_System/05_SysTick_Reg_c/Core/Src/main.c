/**
 * 05_SysTick_Reg_c  —  [계층: 레지스터(CMSIS)]  SysTick 으로 1ms 틱 / millis() / 논블로킹 타이밍
 *
 * HAL 을 사용하지 않는다 (HAL_Init/HAL_Delay 없음). 부팅 후 클럭은 리셋 기본값 HSI 8MHz.
 *
 * 동작
 *   - SysTick 이 1ms 마다 예외를 발생시켜 g_ms 를 증가 (millis 역할).
 *   - 태스크 A: LD2(PA5)를 blink_ms 주기로 토글.
 *   - 태스크 B: 20ms 마다 B1(PC13)을 읽어 2회 연속 눌림이면 blink_ms 를 500 <-> 100 으로 전환(디바운스).
 *   -> while 안에 블로킹 delay 가 없어도 두 작업이 각자의 주기로 동작함을 확인한다.
 *
 * SysTick 레지스터 (Cortex-M3, PM0056 4.5절)
 *   SysTick->LOAD : 리로드 값. 인터럽트 주기 = (LOAD+1) / 클럭.  8MHz 에서 1ms -> LOAD = 8000-1
 *   SysTick->VAL  : 현재 값 (아무 값이나 쓰면 0 으로 클리어)
 *   SysTick->CTRL : bit2 CLKSOURCE(1=HCLK), bit1 TICKINT(1=예외 발생), bit0 ENABLE
 *
 * ISR: SysTick_Handler (아래, 벡터 테이블의 weak 심볼을 재정의). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"

static volatile uint32_t g_ms;          /* ISR 이 쓰고 main 이 읽음 -> volatile 필수 */

/* SysTick 예외 핸들러: 1ms 마다 진입 (SysTick->CTRL.COUNTFLAG 는 읽으면 자동 클리어) */
void SysTick_Handler(void)
{
    g_ms++;
}

static uint32_t millis(void) { return g_ms; }   /* 32비트 읽기는 M3 에서 원자적 */

static void systick_init_1ms(uint32_t hclk_hz)
{
    SysTick->LOAD = hclk_hz / 1000u - 1u;               /* 24비트 레지스터: 최대 16,777,215 */
    NVIC_SetPriority(SysTick_IRQn, 0);                  /* SCB->SHP[11] : 가장 높은 우선순위 */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk          /* bit2: 클럭 = HCLK */
                  | SysTick_CTRL_TICKINT_Msk            /* bit1: 0 도달 시 예외 */
                  | SysTick_CTRL_ENABLE_Msk;            /* bit0: 카운터 시작 */
}

/* 오버플로에 안전한 경과 시간 비교: (now - start) 부호 없는 뺄셈은 49일 래핑에도 올바르다 */
static int elapsed(uint32_t start, uint32_t interval)
{
    return (uint32_t)(millis() - start) >= interval;
}

int main(void)
{
    /* GPIOA/GPIOC 클럭: RCC->APB2ENR.IOPAEN(bit2), IOPCEN(bit4) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    /* PA5 = 출력 푸시풀 2MHz : CRL[23:20] = CNF5(00) MODE5(10) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);

    /* PC13 = 풀업/풀다운 입력 : CRH[23:20] = CNF13(10) MODE13(00). 보드에 외부 풀업이 있다 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);
    GPIOC->BSRR = (1u << 13);                            /* ODR13=1 -> 내부 풀업 선택 */

    systick_init_1ms(SystemCoreClock);                   /* 리셋 후 SystemCoreClock = 8,000,000 */

    uint32_t blink_ms = 500;
    uint32_t t_blink = millis(), t_key = millis();
    uint8_t prev_pressed = 0;

    while (1)
    {
        /* 태스크 A: LED 토글 */
        if (elapsed(t_blink, blink_ms))
        {
            t_blink += blink_ms;                         /* 누적 오차 없이 주기 유지 */
            GPIOA->ODR ^= (1u << 5);                     /* ODR5 반전 */
        }

        /* 태스크 B: 20ms 마다 버튼 샘플링 (누르는 순간 = 이전 샘플 Low 유지 후 현재도 Low 로 확정) */
        if (elapsed(t_key, 20))
        {
            t_key += 20;
            uint8_t pressed = ((GPIOC->IDR & (1u << 13)) == 0);   /* IDR13 == 0 : 눌림 */
            if (pressed && !prev_pressed)
            {
                blink_ms = (blink_ms == 500) ? 100 : 500;
            }
            prev_pressed = pressed;
        }
    }
}

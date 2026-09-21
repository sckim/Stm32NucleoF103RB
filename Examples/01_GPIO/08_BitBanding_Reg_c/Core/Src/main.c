/**
 * 08_BitBanding_Reg_c  —  [계층: 레지스터(CMSIS)]  Cortex-M3 비트 밴딩(bit-banding)으로 단일 비트를 원자적으로 접근
 *
 * 비트 밴딩이란 (PM0056 2.2.5) — Cortex-M3/M4 의 고유 기능 (M0/M0+ 에는 없음)
 *   "비트 밴드 영역"(SRAM 0x20000000~0x200FFFFF, 주변장치 0x40000000~0x400FFFFF)의 각 "비트" 하나가
 *   "별칭(alias) 영역"(0x22000000 / 0x42000000~)의 32비트 워드 하나에 1:1 로 대응한다.
 *   별칭 워드에 0/1 을 쓰면 하드웨어가 해당 비트 하나만 "읽고-수정하고-쓰기"를 원자적으로 수행한다.
 *
 *   alias = alias_base + (byte_offset x 32) + (bit_number x 4)
 *     byte_offset = 대상 주소 - 밴드 기준(0x20000000 또는 0x40000000)
 *
 * 왜 유용한가
 *   레지스터 |= (1<<n) 은 "읽기 -> OR -> 쓰기" 3단계라 그 사이에 ISR 이 같은 레지스터/변수를 바꾸면 갱신이 사라진다(경합).
 *   별칭 쓰기 한 번은 원자적이므로 인터럽트를 막지 않고도 안전하다.
 *   (GPIO 는 BSRR 이 같은 목적을 이미 제공한다. 비트 밴딩은 RCC->APB2ENR, 타이머 CR1.CEN, SRAM 플래그 등에 유용)
 *
 * 이 예제가 하는 일 (HAL 미사용, HSI 8MHz)
 *   1) RCC->APB2ENR 의 IOPAEN(bit2), IOPCEN(bit4) 을 비트 밴딩으로 켠다  -> 클럭 활성화
 *   2) LD2(PA5) 를 GPIOA->ODR bit5 별칭으로 토글하고, B1(PC13) 을 GPIOC->IDR bit13 별칭으로 읽는다
 *   3) SRAM 플래그 워드 하나를 main(bit0)과 SysTick ISR(bit1)이 각자 자기 비트만 비트 밴딩으로 세우고 지운다.
 *      일반 방식(flags |= 1)이었다면 main 이 "읽기"와 "쓰기" 사이에 ISR 이 끼어들 때 ISR 의 bit1 갱신이 덮어써질 수 있어
 *      인터럽트를 잠깐 막아야 하지만, 별칭 쓰기는 한 번의 원자적 접근이라 그럴 필요가 없다.
 *   B1 을 누르고 있으면 LED 가 빠르게, 아니면 느리게 깜빡인다.
 *
 * ISR: SysTick_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>

/* 비트 밴딩 별칭 주소 계산 매크로.  addr: 대상 변수/레지스터 주소,  bit: 0~31 */
#define BITBAND_ALIAS(base_alias, base_band, addr, bit) \
    (*(volatile uint32_t *)((base_alias) + (((uint32_t)(addr) - (base_band)) << 5) + ((bit) << 2)))
#define BB_PERIPH(addr, bit) BITBAND_ALIAS(0x42000000UL, 0x40000000UL, addr, bit)
#define BB_SRAM(addr, bit)   BITBAND_ALIAS(0x22000000UL, 0x20000000UL, addr, bit)

/* SRAM 공유 워드: bit0 = main 이 세움, bit1 = ISR 이 세움 (각자 자기 비트만 비트 밴딩으로 접근) */
static volatile uint32_t flags;
static volatile uint32_t g_ms;

void SysTick_Handler(void)
{
    g_ms++;
    BB_SRAM(&flags, 1) = 1;                   /* bit1 만 원자적으로 세움 (main 이 bit0 를 만지는 중이어도 안전) */
}

int main(void)
{
    /* (1) 클럭 활성화: 일반 방식은 RCC->APB2ENR |= (1<<2)|(1<<4),  비트 밴딩은 비트마다 한 번의 원자적 쓰기 */
    BB_PERIPH(&RCC->APB2ENR, 2) = 1;          /* IOPAEN */
    BB_PERIPH(&RCC->APB2ENR, 4) = 1;          /* IOPCEN */

    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);     /* PA5 출력 2MHz */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x8u << 20);     /* PC13 입력(풀업 선택) */
    GPIOC->BSRR = (1u << 13);

    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    uint32_t t_led = 0;
    uint32_t ticks_seen = 0;                  /* main 이 소비한 ISR 이벤트 수 (디버거에서 g_ms 와 비교해 보면 된다) */
    (void)ticks_seen;
    while (1)
    {
        /* (2) B1: IDR 비트 13 을 별칭으로 읽는다 (0 = 눌림).  결과는 항상 0 또는 1 */
        uint32_t interval = (BB_PERIPH(&GPIOC->IDR, 13) == 0) ? 100u : 500u;

        if ((uint32_t)(g_ms - t_led) >= interval)
        {
            t_led = g_ms;
            BB_PERIPH(&GPIOA->ODR, 5) ^= 1u;                       /* 별칭 워드 읽고 반전해 쓰기: ODR5 하나만 바뀜 */
        }

        /* (3) main 이 bit0 를 세우고, ISR 이 세운 bit1 을 확인한 뒤 자기 손으로 비운다 (둘 다 단일 원자 접근) */
        BB_SRAM(&flags, 0) = 1;
        if (BB_SRAM(&flags, 1))
        {
            BB_SRAM(&flags, 1) = 0;               /* ISR 이 세운 1ms 이벤트를 소비 */
            ticks_seen++;
        }
    }
}

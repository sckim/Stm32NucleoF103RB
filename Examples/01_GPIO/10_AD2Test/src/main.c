#include "stm32f1xx.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint8_t pin;
} GpioPin;

// NUCLEO-F103RB의 D2~D15 디지털 핀과 A4/A5 아날로그 헤더 핀.
// 이 보드에서 D14/D15(PB9/PB8)와 A4/A5(PC1/PC0)는 서로 다른 핀이다.
// D0(PA3)/D1(PA2)는 USART2 RX/TX와 겹치므로 순차 GPIO 출력에서 제외한다.
static const GpioPin arduino_pins[] = {
    {GPIOA, 10}, // D2  = PA10
    {GPIOB, 3},  // D3  = PB3
    {GPIOB, 5},  // D4  = PB5
    {GPIOB, 4},  // D5  = PB4
    {GPIOB, 10}, // D6  = PB10
    {GPIOA, 8},  // D7  = PA8
    {GPIOA, 9},  // D8  = PA9
    {GPIOC, 7},  // D9  = PC7
    {GPIOB, 6},  // D10 = PB6
    {GPIOA, 7},  // D11 = PA7
    {GPIOA, 6},  // D12 = PA6
    {GPIOA, 5},  // D13 = PA5 (온보드 LD2)
    // {GPIOB, 9},  // D14 = PB9 (SDA)
    // {GPIOB, 8},  // D15 = PB8 (SCL)
    {GPIOC, 1},  // A4  = PC1 (아날로그 헤더)
    {GPIOC, 0},  // A5  = PC0 (아날로그 헤더)
};

#define GPIO_PIN_COUNT (sizeof(arduino_pins) / sizeof(arduino_pins[0]))
#define UART_BAUD_RATE 9600U

static volatile uint32_t msTicks = 0;

// SysTick 인터럽트 서비스 루틴: 1ms마다 시스템 경과 시간을 갱신한다.
void SysTick_Handler(void)
{
    msTicks++;
}

static void delay_ms(uint32_t duration)
{
    const uint32_t start = msTicks;
    while ((msTicks - start) < duration)
    {
        __NOP();
    }
}

static void gpio_init(void)
{
    // GPIOA/B/C와 AFIO의 APB2 클럭을 활성화한다.
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN |
                    RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN |
                    RCC_APB2ENR_AFIOEN;

    // D3(PB3)를 GPIO로 사용: AFIO_MAPR.SWJ_CFG[2:0] = 010으로
    // JTAG-DP를 비활성화하고 SW-DP(ST-LINK 디버깅)는 유지한다.
    AFIO->MAPR = (AFIO->MAPR & ~(0x7UL << 24)) | (0x2UL << 24);

    for (uint32_t i = 0; i < GPIO_PIN_COUNT; i++)
    {
        GPIO_TypeDef *port = arduino_pins[i].port;
        const uint32_t pin = arduino_pins[i].pin;
        volatile uint32_t *config = (pin < 8U) ? &port->CRL : &port->CRH;
        const uint32_t shift = (pin & 7U) * 4U;

        // 출력 모드 설정 전 LOW를 써서 초기 상태를 끈다.
        port->BSRR = 1UL << (pin + 16U);

        // GPIOx_CRL/CRH의 해당 핀 필드: MODE=01(10MHz), CNF=00(푸시풀 출력).
        *config = (*config & ~(0xFUL << shift)) | (0x1UL << shift);
    }
}

static void usart2_init(void)
{
    // USART2는 APB1, GPIOA는 APB2에 연결된다.
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PA2(D1)를 USART2_TX 대체 기능 푸시풀 출력으로 설정.
    // GPIOA 클럭은 gpio_init()에서 이미 활성화되었다.
    GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << (2U * 4U))) |
                 (0xBUL << (2U * 4U)); // MODE=11(50MHz), CNF=10(AF 푸시풀)

    // RCC_CFGR.PPRE1[2:0] 값 0~3은 /1, 4~7은 /2,/4,/8,/16 분주를 뜻한다.
    const uint32_t ppre1 = (RCC->CFGR >> 8U) & 0x7U;
    const uint32_t pclk1 = SystemCoreClock >> ((ppre1 < 4U) ? 0U : (ppre1 - 3U));

    USART2->CR1 = 0U;
    USART2->BRR = (pclk1 + (UART_BAUD_RATE / 2U)) / UART_BAUD_RATE;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE; // 송신기와 USART 활성화, 8N1
}

static void usart2_write(const char *text)
{
    while (*text != '\0')
    {
        while ((USART2->SR & USART_SR_TXE) == 0U)
        {
        }
        USART2->DR = (uint8_t)*text++;
    }

    while ((USART2->SR & USART_SR_TC) == 0U)
    {
    }
}

int main(void)
{
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        while (1)
        {
        }
    }

    gpio_init();
    usart2_init();

    while (1)
    {
        for (uint32_t i = 0; i < GPIO_PIN_COUNT; i++)
        {
            const GpioPin pin = arduino_pins[i];

            // 현재 핀을 100ms 켠 뒤 바로 끄고, 추가 대기 없이 다음 핀으로 이동한다.
            pin.port->BSRR = 1UL << pin.pin;
            delay_ms(1U);
            pin.port->BSRR = 1UL << (pin.pin + 16U);
        }

        // D2~D15와 A4/A5 순차 출력이 끝날 때마다 D1(PA2/USART2_TX)으로 메시지 전송.
        usart2_write("Hello, AD2\r\n");
    }
}

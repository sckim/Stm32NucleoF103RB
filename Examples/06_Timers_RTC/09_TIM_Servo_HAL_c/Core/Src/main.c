/**
 * 09_TIM_Servo_HAL_c  —  [계층: HAL]  RC 서보 제어: 50Hz PWM, 펄스 폭 1.0~2.0ms = 각도 0~180°
 *
 * 서보 프로토콜 (SG90/MG90S 등 호비 서보)
 *   20ms(50Hz) 주기의 펄스, **High 폭이 각도를 정한다**:  1.0ms = 0°(또는 −90°),  1.5ms = 중앙,  2.0ms = 180°(또는 +90°).
 *   서보마다 유효 범위가 조금씩 달라(0.5~2.5ms 까지 도는 것도 있음) 기계적 한계에 닿아 떠는 소리가 나면 범위를 좁힌다.
 *   PWM 주파수 자체는 정밀할 필요가 없고(40~60Hz 허용), **펄스 폭의 정확도**가 각도 정확도다 -> 타이머가 하드웨어로 만들어야 하는 이유.
 *
 * 타이머 계산: TIM2_CH1 (PA0), TIM2 클럭 64MHz
 *   PSC = 64-1  -> 카운터 1MHz (1us)     ARR = 20000-1 -> 주기 20ms = 50Hz    CCR1 = 펄스 폭[us] = 1000 + angle x 1000 / 180
 *   -> 분해능 1us = 약 0.18°.  더 세밀하게 하려면 PSC 를 줄여(예: /8) 카운터를 8MHz 로 하고 ARR = 160000-1 는 16비트를 넘으므로 안 된다
 *      (ARR 최대 65535 이므로 20ms 주기에는 1MHz 가 한계에 가깝다: 20000 카운트).
 *
 * 배선: 서보 신호선(주황/노랑) = PA0,  전원선(빨강) = **외부 5V**,  GND(갈색/검정) = 보드 GND 와 공통
 *   *** 서보 전원을 Nucleo 의 3.3V/5V 핀에서 끌어 쓰면 움직일 때 전류 스파이크로 보드가 리셋될 수 있다. 외부 전원 + GND 공통으로 연결. ***
 *   신호는 3.3V 레벨이라도 대부분 서보가 인식한다.
 *
 * 동작
 *   - 기본: 0° -> 180° -> 0° 를 부드럽게 반복 (10ms 마다 1° 이동, 약 3.6초에 왕복)
 *   - B1(PC13)을 누를 때마다 모드 전환: 스윕 -> 중앙 90° 고정 -> 0° 고정 -> 180° 고정 -> 스윕 ...
 *   - 현재 각도/펄스 폭을 UART(115200)로 출력.  (고정 모드에서는 PWM 이 계속 나와야 서보가 위치를 유지한다)
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define PULSE_MIN_US 1000U             /* 0°   */
#define PULSE_MAX_US 2000U             /* 180° */

TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_PWM_Init(void);

static uint32_t servo_write_angle(uint32_t deg)
{
    if (deg > 180U) deg = 180U;
    uint32_t us = PULSE_MIN_US + deg * (PULSE_MAX_US - PULSE_MIN_US) / 180U;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, us);                 /* TIM2->CCR1 = 펄스 폭[us] */
    return us;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM2_PWM_Init();

    static const char *const mode_name[4] = { "sweep 0<->180", "center 90", "fixed 0", "fixed 180" };
    uint32_t mode = 0, deg = 0;
    int dir = 1;
    char msg[64];
    uint32_t t_step = HAL_GetTick(), t_print = HAL_GetTick();

    while (1)
    {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1: 모드 전환 */
        {
            HAL_Delay(30);
            mode = (mode + 1) % 4;
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(30);
        }

        if (mode == 0)
        {
            if (HAL_GetTick() - t_step >= 10)                          /* 10ms 마다 1° */
            {
                t_step += 10;
                if (dir > 0) { if (deg >= 180) dir = -1; else deg++; }
                else         { if (deg == 0)   dir = 1;  else deg--; }
            }
        }
        else deg = (mode == 1) ? 90 : (mode == 2 ? 0 : 180);

        uint32_t us = servo_write_angle(deg);

        if (HAL_GetTick() - t_print >= 500)
        {
            t_print += 500;
            int n = snprintf(msg, sizeof msg, "[%s] angle=%3lu deg  pulse=%lu us\r\n", mode_name[mode], (unsigned long)deg, (unsigned long)us);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
}

static void MX_TIM2_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* TIM2_CH1 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;                      /* 1MHz */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 20000 - 1;                      /* 20ms = 50Hz */
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 1500;                                    /* 시작은 중앙 */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;    /* B1 */
    HAL_GPIO_Init(GPIOC, &g);
}

/* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용 MSP: 클럭/핀만 설정 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();   /* RCC->APB1ENR.USART2EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN   */
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX: CNF=10, MODE=11 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX: CNF=01 (플로팅 입력) */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* HSI(8MHz)/2 x16 = 64MHz. Nucleo 는 HSE 가 ST-Link 에서 오므로 HSI 로 단순화 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    o.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();

    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2;          /* APB1 = 32MHz (최대 36MHz) */
    c.APB2CLKDivider = RCC_HCLK_DIV1;          /* APB2 = 64MHz */
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

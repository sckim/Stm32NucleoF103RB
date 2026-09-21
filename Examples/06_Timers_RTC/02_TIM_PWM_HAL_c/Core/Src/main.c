/**
 * 02_TIM_PWM_HAL_c  —  [계층: HAL]  TIM2 CH1 PWM 출력 (LED 밝기 조절 / 서보 응용)
 *
 * 동작
 *   - TIM2_CH1(PA0 = Nucleo 'A0')에 1kHz PWM 출력, 듀티를 0~100% 왕복시켜 LED 를 숨쉬듯 점멸.
 *   - 회로: PA0 -> 저항(330Ω) -> 외부 LED -> GND
 *     (온보드 LD2 는 PA5 = TIM 채널이 아니므로 PWM 불가)
 *
 * PWM 주파수/듀티 계산
 *   TIM2 클럭 = 64MHz (APB1=32MHz, 분주비 != 1 이므로 타이머 클럭은 x2)
 *   PSC = 64-1   -> 카운터 클럭 1MHz  (TIM2->PSC)
 *   ARR = 1000-1 -> 주기 1000 카운트 = 1ms = 1kHz  (TIM2->ARR)
 *   CCR1 = 듀티(0~1000)  (TIM2->CCR1),  듀티% = CCR1 / (ARR+1) x 100
 *   CCMR1.OC1M = 110(PWM mode 1), CCER.CC1E = 1 (출력 활성)
 *
 * 서보 응용: PSC=64-1, ARR=20000-1 (20ms=50Hz), CCR1=1000~2000 (1~2ms 펄스)
 *
 * 이 예제는 인터럽트를 쓰지 않는다 (PWM 은 타이머 하드웨어가 파형을 생성).
 */
#include "main.h"

TIM_HandleTypeDef htim2;

void SystemClock_Config(void);
static void MX_TIM2_PWM_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_TIM2_PWM_Init();

    int32_t duty = 0, step = 1;
    while (1)
    {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)duty);   /* TIM2->CCR1 = duty */

        duty += step;
        if (duty >= 999) step = -1;
        if (duty <= 0)   step = 1;
        HAL_Delay(1);                                                    /* 1ms x 1000 = 1초에 0->100% */
    }
}

/* HAL_TIM_PWM_Init 이 호출: 타이머 클럭 활성화 (핀은 MX_TIM2_PWM_Init 에서 설정) */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();    /* RCC->APB1ENR.TIM2EN */
    }
}

static void MX_TIM2_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;                 /* TIM2_CH1 (기본 매핑, AFIO 리맵 불필요) */
    g.Mode = GPIO_MODE_AF_PP;           /* CRL.CNF0=10, MODE0=11 */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;   /* CR1.ARPE */
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;        /* CNT < CCR1 이면 High */
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();   /* CCER.CC1E=1, CR1.CEN=1 */
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

/**
 * 52_TIM_InputCapture_HAL_c  —  [계층: HAL]  TIM2 입력 캡처로 주파수 측정
 *
 * 동작
 *   - TIM3_CH1(PA6, 'D12')이 1kHz/50% PWM 테스트 신호를 출력한다.
 *   - TIM2_CH1(PA0, 'A0')이 상승 에지마다 CNT 값을 CCR1 에 래치(캡처)한다.
 *   - 연속된 두 캡처 값의 차 = 신호 주기(카운트) -> 주파수 계산 후 USART2(115200)로 0.5초마다 출력.
 *
 * 배선: PA6(D12) ---- 점퍼선 ---- PA0(A0)   (다른 신호를 측정하려면 PA0 에 연결)
 *
 * 측정 범위 계산
 *   TIM2 카운터 클럭 = 64MHz/64 = 1MHz (1us 분해능), 카운터 16비트(ARR=0xFFFF)
 *   최저 측정 주파수 = 1MHz/65536 = 약 15.3Hz,  주기 차 계산은 uint16_t 뺄셈으로 오버플로 1회를 자동 처리
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler()
 *     -> HAL_TIM_IC_CaptureCallback() (이 파일)  : SR.CC1IF 발생 시 호출, ISR 문맥이므로 짧게 처리
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

TIM_HandleTypeDef htim2;     /* 입력 캡처 */
TIM_HandleTypeDef htim3;     /* 테스트용 PWM */
UART_HandleTypeDef huart2;

static volatile uint16_t period_ticks;   /* 콜백에서 계산한 마지막 주기 [us] */
static volatile uint8_t  have_sample;    /* 마지막 출력 이후 새 캡처가 있었는가 */
static uint16_t last_capture;
static uint8_t  first_edge = 1;

void SystemClock_Config(void);
static void MX_TIM3_PWM_Init(void);
static void MX_TIM2_IC_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_TIM3_PWM_Init();
    MX_TIM2_IC_Init();

    char msg[64];
    while (1)
    {
        HAL_Delay(500);

        /* ISR 과 공유하는 변수를 일관되게 복사하기 위해 잠깐 인터럽트를 막는다 */
        __disable_irq();
        uint16_t p = period_ticks;
        uint8_t ok = have_sample;
        have_sample = 0;
        __enable_irq();

        int n;
        if (ok && p != 0)
            n = snprintf(msg, sizeof msg, "period=%u us  freq=%lu Hz\r\n", (unsigned)p, (unsigned long)(1000000UL / p));
        else
            n = snprintf(msg, sizeof msg, "no signal on PA0\r\n");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님, HAL_TIM_IRQHandler 가 호출)                              */
/* ------------------------------------------------------------------------- */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        uint16_t now = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);   /* TIM2->CCR1 */
        if (first_edge)
        {
            first_edge = 0;                        /* 첫 에지는 기준점일 뿐 */
        }
        else
        {
            period_ticks = (uint16_t)(now - last_capture);   /* 16비트 래핑 뺄셈 */
            have_sample = 1;
        }
        last_capture = now;
    }
}

/* ---- MSP: 클럭/NVIC (핀은 각 Init 함수에서 설정) ---- */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();   /* RCC->APB1ENR.TIM3EN */
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();                  /* RCC->APB1ENR.TIM2EN */
        HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);                /* NVIC->ISER[0] bit28 */
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX */
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

/* TIM3_CH1 = PA6 : 1kHz, 듀티 50% (PSC=64-1 -> 1MHz, ARR=1000-1, CCR1=500) */
static void MX_TIM3_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_6;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 64 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 500;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

/* TIM2_CH1 = PA0 : 상승 에지 입력 캡처 (CCMR1.CC1S=01 TI1 직접, CCER.CC1P=0 상승) */
static void MX_TIM2_IC_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_IC_InitTypeDef ic = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;   /* CNF0=01 플로팅 입력 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;                 /* 1MHz -> 1us 분해능 */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK) Error_Handler();

    ic.ICPolarity = TIM_ICPOLARITY_RISING;
    ic.ICSelection = TIM_ICSELECTION_DIRECTTI;
    ic.ICPrescaler = TIM_ICPSC_DIV1;
    ic.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &ic, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();   /* DIER.CC1IE=1, CCER.CC1E=1, CR1.CEN=1 */
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

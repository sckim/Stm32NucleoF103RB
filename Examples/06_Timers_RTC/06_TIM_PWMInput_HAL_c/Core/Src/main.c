/**
 * 06_TIM_PWMInput_HAL_c  —  [계층: HAL]  타이머 PWM 입력 모드: 하드웨어가 주기와 듀티를 동시에 측정
 *
 * 03_TIM_InputCapture(입력 캡처)은 CPU 가 두 캡처 값을 빼서 주기를 계산했다. PWM 입력 모드는 그것을 하드웨어로 자동화한다.
 *
 * 원리 (RM0008 15.3.6)  TIM2, 입력 핀 PA0(TI1)
 *   CH1 : TI1FP1  상승 에지에 캡처 -> CCR1 = 주기(카운트)  + 슬레이브 모드 "리셋" -> 상승 에지마다 카운터를 0 으로 리셋
 *   CH2 : TI1FP2  하강 에지에 캡처 (간접 선택, CC2S=10) -> CCR2 = 하이 구간 폭(카운트)
 *   => 상승 에지마다 카운터가 0 에서 다시 시작하므로, CCR1 이 곧 주기, CCR2 가 곧 펄스 폭. 나눗셈 한 번이면 듀티.
 *   TIM2->SMCR : SMS=100(리셋 모드), TS=101(TI1FP1)
 *
 * 동작
 *   - TIM3_CH1(PA6, 'D12')이 1kHz PWM 테스트 신호를 출력하고, B1(PC13)을 누를 때마다 듀티가 25% -> 50% -> 75% 로 바뀐다.
 *   - PA6 -> PA0 점퍼선을 연결하면 TIM2 가 주파수/듀티를 측정해 UART(115200)로 출력한다. (외부 신호 측정 시 PA0 에 직접 연결)
 *   - 측정 범위: 카운터 클럭 1MHz(1us), 16비트 -> 최저 약 15.3Hz. 카운트가 클수록 분해능이 좋으므로 입력 주파수에 맞춰 PSC 를 조정.
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler() -> HAL_TIM_IC_CaptureCallback() (이 파일)
 *   (CH1 캡처 = 주기 한 개가 완성된 시점. 이때 CCR2 도 같은 주기의 하이 폭이 이미 들어 있다)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

TIM_HandleTypeDef htim2;      /* PWM 입력 측정 */
TIM_HandleTypeDef htim3;      /* 테스트 신호 발생 */
UART_HandleTypeDef huart2;

static volatile uint32_t period_ticks, high_ticks;
static volatile uint8_t have_sample;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_PWM_Init(void);
static void MX_TIM2_PWMInput_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM3_PWM_Init();
    MX_TIM2_PWMInput_Init();

    static const uint16_t duty_ccr[3] = { 250, 500, 750 };           /* 25%, 50%, 75% (ARR=999+1 기준) */
    uint8_t di = 1;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty_ccr[di]);

    char msg[80];
    uint32_t t_print = HAL_GetTick();
    while (1)
    {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)  /* B1: 듀티 변경 */
        {
            HAL_Delay(30);
            di = (uint8_t)((di + 1) % 3);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty_ccr[di]);
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        }

        if (HAL_GetTick() - t_print >= 500)
        {
            t_print += 500;
            __disable_irq();
            uint32_t p = period_ticks, h = high_ticks;
            uint8_t ok = have_sample;
            have_sample = 0;
            __enable_irq();

            int n;
            if (ok && p > 0)
                n = snprintf(msg, sizeof msg, "period=%lu us  freq=%lu Hz  high=%lu us  duty=%lu%% (set %u%%)\r\n",
                             (unsigned long)p, (unsigned long)(1000000UL / p), (unsigned long)h,
                             (unsigned long)(h * 100UL / p), (unsigned)(duty_ccr[di] / 10U));
            else
                n = snprintf(msg, sizeof msg, "no signal on PA0\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): CH1 캡처 = 상승 에지에서 주기 한 개 완성                      */
/* ------------------------------------------------------------------------- */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        period_ticks = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);   /* TIM2->CCR1 (상승->상승) */
        high_ticks   = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);   /* TIM2->CCR2 (상승->하강) */
        have_sample = 1;
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

/* TIM3_CH1 = PA6 : 1kHz (PSC=64-1 -> 1MHz, ARR=1000-1) */
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

static void MX_TIM2_PWMInput_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_IC_InitTypeDef ic = {0};
    TIM_SlaveConfigTypeDef sl = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;           /* TI1 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;                       /* 1MHz -> 1us */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK) Error_Handler();

    ic.ICPolarity = TIM_ICPOLARITY_RISING;               /* CH1: 상승 에지, TI1 직접 (CC1S=01) */
    ic.ICSelection = TIM_ICSELECTION_DIRECTTI;
    ic.ICPrescaler = TIM_ICPSC_DIV1;
    ic.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &ic, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    ic.ICPolarity = TIM_ICPOLARITY_FALLING;              /* CH2: 하강 에지, TI1 간접 (CC2S=10) */
    ic.ICSelection = TIM_ICSELECTION_INDIRECTTI;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &ic, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

    sl.SlaveMode = TIM_SLAVEMODE_RESET;                  /* SMCR.SMS = 100 : 트리거마다 카운터 리셋 */
    sl.InputTrigger = TIM_TS_TI1FP1;                     /* SMCR.TS  = 101 : TI1FP1 (상승 에지) */
    sl.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sl.TriggerPrescaler = TIM_ICPSC_DIV1;
    sl.TriggerFilter = 0;
    if (HAL_TIM_SlaveConfigSynchro(&htim2, &sl) != HAL_OK) Error_Handler();

    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();   /* CH1 인터럽트: 주기 완성 통지 */
    if (HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_2) != HAL_OK) Error_Handler();      /* CH2 는 값만 캡처 */
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;      /* B1 */
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

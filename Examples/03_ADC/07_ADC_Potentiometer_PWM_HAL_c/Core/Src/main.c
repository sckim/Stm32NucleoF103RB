/**
 * 07_ADC_Potentiometer_PWM_HAL_c  —  [계층: HAL]  가변저항으로 LED 밝기 조절: ADC(입력) + TIM PWM(출력) + 감마 보정
 *
 * "센서 값 -> 처리 -> 액추에이터" 를 가장 단순하게 보여 주는 예제.
 *   입력 : PA1 (ADC1_IN1) 에 가변저항 가운데 단자, 양 끝은 3.3V / GND.  연속 변환 + DMA 로 최신값을 항상 변수에 유지.
 *   처리 : 1) 이동 평균(8샘플)으로 노이즈 억제  2) 히스테리시스(±2)로 값이 미세하게 떨릴 때 출력이 깜빡이지 않게
 *          3) 감마 보정(밝기 ≈ 입력^2): 사람 눈은 밝기를 로그에 가깝게 느끼므로 선형 듀티는 낮은 쪽이 너무 급하게 밝아진다.
 *   출력 : TIM2_CH1 (PA0) 1kHz PWM -> 저항(330Ω) -> 외부 LED -> GND.   (온보드 LD2 는 PA5 = PWM 채널이 아님)
 *
 * PWM: TIM2 64MHz/64 = 1MHz, ARR = 1000-1, CCR1 = 0..1000 (듀티 0..100%).  ADC 12비트(0..4095) -> 감마 -> 0..1000
 *   duty = (adc/4095)^2 x 1000 = adc x adc / 16765 (정수 연산, adc = 4095 일 때 1000)
 *
 * ISR: 인터럽트를 사용하지 않는다 (ADC 는 DMA 가 최신값을 갱신, main 이 20ms 마다 읽는다).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

static volatile uint16_t adc_val;               /* DMA 가 계속 덮어쓰는 최신 변환값 */

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_PWM_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_TIM2_PWM_Init();
    MX_ADC1_Init();
    HAL_ADCEx_Calibration_Start(&hadc1);
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1) != HAL_OK) Error_Handler();

    uint32_t hist[8] = {0}, sum = 0;
    uint32_t idx = 0, filtered = 0, last_out = 0xFFFF;
    char msg[64];
    uint32_t t_print = HAL_GetTick();

    while (1)
    {
        HAL_Delay(20);

        /* 1) 이동 평균 (링 버퍼로 합계를 유지) */
        sum -= hist[idx];
        hist[idx] = adc_val;
        sum += hist[idx];
        idx = (idx + 1) & 7;
        filtered = sum >> 3;

        /* 2) 감마 보정 후 3) 히스테리시스: 새 값이 이전 출력과 2 이상 다를 때만 갱신 */
        uint32_t duty = (filtered * filtered) / 16765U;
        if (duty > 1000U) duty = 1000U;
        if (last_out == 0xFFFF || (duty > last_out + 2U) || (duty + 2U < last_out) || duty == 0U || duty == 1000U)
        {
            last_out = duty;
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty);           /* TIM2->CCR1 */
        }

        if (HAL_GetTick() - t_print >= 500)
        {
            t_print += 500;
            int n = snprintf(msg, sizeof msg, "adc=%4lu  duty=%3lu.%lu%%\r\n", (unsigned long)filtered,
                             (unsigned long)(last_out / 10), (unsigned long)(last_out % 10));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    g.Pin = GPIO_PIN_1;  g.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &g);

    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_DISABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};
    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;                       /* 연속 변환 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_1;  ch.Rank = ADC_REGULAR_RANK_1;  ch.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
}

static void MX_TIM2_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;     /* TIM2_CH1 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();
    oc.OCMode = TIM_OCMODE_PWM1;  oc.Pulse = 0;  oc.OCPolarity = TIM_OCPOLARITY_HIGH;  oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
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

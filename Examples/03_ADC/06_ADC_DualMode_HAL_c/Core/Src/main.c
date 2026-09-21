/**
 * 06_ADC_DualMode_HAL_c  —  [계층: HAL]  ADC 듀얼 모드(동시 정규 변환): ADC1 과 ADC2 가 같은 순간에 두 신호를 샘플링
 *
 * 왜 듀얼 모드인가
 *   ADC 하나로 채널 두 개를 번갈아 읽으면 두 샘플 사이에 변환 시간만큼 "시간차(skew)"가 생긴다.
 *   전압/전류의 순간 곱(전력), 위상 차, I/Q 신호처럼 **동시성**이 중요한 측정에는 두 ADC 를 동시에 트리거해야 한다.
 *   F103 은 ADC1 + ADC2 두 개를 가지고 있어 "정규 동시 모드"(ADC_DUALMODE_REGSIMULT)를 쓸 수 있다 (RM0008 11.9.2 Regular simultaneous mode).
 *
 * 동작 원리
 *   마스터 ADC1 이 TIM3 TRGO(1kHz)로 트리거되면 슬레이브 ADC2 도 같은 순간에 변환을 시작한다.
 *   두 결과는 ADC1->DR 하나에 32비트로 합쳐진다:  하위 16비트 = ADC1(CH0, PA0),  상위 16비트 = ADC2(CH1, PA1)
 *   -> DMA 가 32비트(WORD) 단위로 버퍼에 저장하므로 DMA 한 번으로 두 채널 데이터를 얻는다 (ADC2 는 자기 DMA 가 없다).
 *   (ADC->CR1.DUALMOD[19:16] = 0110 : 정규 동시 모드)
 *
 * 이 예제
 *   PA0, PA1 의 전압을 1kHz 로 동시 샘플링, DMA 원형 버퍼(64워드), 반/전체 콜백에서 채널별 평균 계산, 1초마다 UART(115200) 출력.
 *   두 입력을 같은 신호원에 연결하면 두 값이 거의 같아야 한다(동시 샘플이므로). 다른 전압에 연결하면 각자의 값이 나온다.
 *
 * ISR vs 콜백
 *   DMA1_Channel1_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> HAL_ADC_ConvHalfCpltCallback() / HAL_ADC_ConvCpltCallback() (이 파일)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define N_WORDS 64                              /* 원형 버퍼 워드 수 (반 = 32) */

ADC_HandleTypeDef hadc1;                        /* 마스터 */
ADC_HandleTypeDef hadc2;                        /* 슬레이브 */
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

static uint32_t buf[N_WORDS];                   /* [ADC2:ADC1] 32비트 워드 */
static volatile uint32_t avg1, avg2, half_events, full_events;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC_Dual_Init(void);

/* 반 버퍼(32워드)에서 채널별 평균. ISR 문맥이므로 짧게 */
static void process_half(const uint32_t *p)
{
    uint32_t s1 = 0, s2 = 0;
    for (uint32_t i = 0; i < N_WORDS / 2; i++)
    {
        s1 += p[i] & 0xFFFFu;                   /* 하위 16비트 = ADC1 */
        s2 += p[i] >> 16;                       /* 상위 16비트 = ADC2 */
    }
    avg1 = s1 / (N_WORDS / 2);
    avg2 = s2 / (N_WORDS / 2);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_TIM3_Init();
    MX_ADC_Dual_Init();

    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc2);
    /* ADC2 를 먼저 켜고(슬레이브 대기), ADC1 이 트리거를 받을 때 둘이 함께 변환한다. 결과는 32비트 워드로 DMA 전송 */
    if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, buf, N_WORDS) != HAL_OK) Error_Handler();
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) Error_Handler();              /* TRGO 1kHz */

    char msg[96];
    uint32_t last_h = 0;
    while (1)
    {
        HAL_Delay(1000);
        uint32_t h = half_events;
        int n = snprintf(msg, sizeof msg, "ADC1(PA0)=%4lu  ADC2(PA1)=%4lu  diff=%+ld  (half events %lu/s, full %lu)\r\n",
                         (unsigned long)avg1, (unsigned long)avg2, (long)avg1 - (long)avg2,
                         (unsigned long)(h - last_h), (unsigned long)full_events);
        last_h = h;
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥)                                                          */
/* ------------------------------------------------------------------------- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) { process_half(&buf[0]); half_events++; }
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) { process_half(&buf[N_WORDS / 2]); full_events++; }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    GPIO_InitTypeDef g = {0};
    if (hadc->Instance == ADC1)
    {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();
        g.Pin = GPIO_PIN_0 | GPIO_PIN_1;  g.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &g);

        hdma_adc1.Instance = DMA1_Channel1;                          /* 듀얼 모드의 DMA 는 ADC1 채널만 쓴다 */
        hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;    /* 32비트: ADC2 결과가 상위 16비트에 함께 들어옴 */
        hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        hdma_adc1.Init.Mode = DMA_CIRCULAR;
        hdma_adc1.Init.Priority = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();
        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
        HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    }
    else if (hadc->Instance == ADC2)
    {
        __HAL_RCC_ADC2_CLK_ENABLE();                             /* RCC->APB2ENR.ADC2EN */
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
}

static void MX_TIM3_Init(void)
{
    TIM_MasterConfigTypeDef m = {0};
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 64 - 1;                               /* 1MHz */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;                                /* 1kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();
    m.MasterOutputTrigger = TIM_TRGO_UPDATE;
    m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &m) != HAL_OK) Error_Handler();
}

static void MX_ADC_Dual_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    ADC_MultiModeTypeDef mm = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};

    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;                    /* 10.67MHz */
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    /* ADC1 (마스터): TIM3 TRGO 로 트리거, CH0 */
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_0;  ch.Rank = ADC_REGULAR_RANK_1;  ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();

    /* ADC2 (슬레이브): 소프트웨어 시작으로 설정 (마스터가 동시 트리거), CH1 */
    hadc2.Instance = ADC2;
    hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc2.Init.ContinuousConvMode = DISABLE;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc2.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc2) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_1;
    if (HAL_ADC_ConfigChannel(&hadc2, &ch) != HAL_OK) Error_Handler();   /* 샘플 시간은 두 ADC 가 같아야 동기가 맞는다 */

    mm.Mode = ADC_DUALMODE_REGSIMULT;                            /* CR1.DUALMOD = 0110 */
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &mm) != HAL_OK) Error_Handler();
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

/**
 * 21_ADC_DMA_TimTrigger_HAL_c  —  [계층: HAL]  타이머 트리거 + ADC 스캔 + DMA 원형 저장
 *
 * 동작 (CPU 개입 없이 정확한 주기로 샘플링)
 *   TIM3 업데이트 이벤트(1kHz) --TRGO--> ADC1 외부 트리거 --> CH0(PA0), CH1(PA1) 2채널 스캔 변환
 *   --> DMA1 채널1 이 결과를 adc_buf[] 에 원형(circular)으로 저장
 *   --> 버퍼 절반(HT)/끝(TC)마다 콜백 -> 평균 계산. 1초마다 UART 출력.
 *
 * 왜 타이머 트리거인가: 소프트웨어 시작/연속 변환은 샘플 간격이 흔들리거나 변환시간에 좌우된다.
 *   TRGO 트리거는 샘플링 주파수(1kHz)가 타이머로 정확히 정해져 FFT/필터 같은 신호처리에 적합하다.
 *
 * 핀: PA0(A0), PA1(A1)에 가변저항 또는 0~3.3V 신호 연결 (미연결 시 플로팅 값이 나온다)
 *
 * 하드웨어 대응 (RM0008)
 *   TIM3->CR2.MMS = 010 (TRGO = 업데이트 이벤트)
 *   ADC1->CR2: EXTSEL = 100 (TIM3_TRGO), EXTTRIG = 1, DMA = 1, ADON, (CONT = 0)
 *   ADC1->CR1.SCAN = 1, ADC1->SQR1.L = 1 (변환 2개), SQR3 = {CH0, CH1}, SMPR2 = 55.5 사이클
 *   ADC 클럭 = PCLK2(64MHz)/6 = 10.67MHz (최대 14MHz)
 *   DMA1_Channel1: 주변장치->메모리, 반워드, 원형, HT/TC 인터럽트
 *
 * ISR vs 콜백
 *   DMA1_Channel1_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> HAL_ADC_ConvHalfCpltCallback() / HAL_ADC_ConvCpltCallback() (이 파일)
 */
#include "main.h"
#include <stdio.h>

#define N_CH        2
#define N_SCAN      32                       /* 절반 버퍼당 스캔 횟수 (1kHz -> 32ms) */
#define BUF_LEN     (N_CH * N_SCAN * 2)      /* 2 (절반) x 32 스캔 x 2 채널 = 128 반워드 */

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

static uint16_t adc_buf[BUF_LEN];            /* [ch0, ch1, ch0, ch1, ...] 인터리브 */
static volatile uint32_t avg_ch[N_CH];       /* 최근 절반 버퍼의 채널별 평균 */
static volatile uint32_t half_events, full_events;

void SystemClock_Config(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);

/* 절반 버퍼(offset 부터 N_CH*N_SCAN 개)의 채널별 평균. ISR 문맥에서 호출되므로 짧게 */
static void process_half(const uint16_t *p)
{
    uint32_t sum[N_CH] = {0};
    for (uint32_t i = 0; i < N_SCAN; i++)
        for (uint32_t c = 0; c < N_CH; c++)
            sum[c] += p[i * N_CH + c];
    for (uint32_t c = 0; c < N_CH; c++) avg_ch[c] = sum[c] / N_SCAN;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_TIM3_Init();
    MX_ADC1_Init();

    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();   /* ADC1->CR2.CAL */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, BUF_LEN) != HAL_OK) Error_Handler();
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) Error_Handler();            /* 인터럽트 없이 TRGO 만 사용 */

    char msg[96];
    uint32_t last_h = 0, last_f = 0;
    while (1)
    {
        HAL_Delay(1000);
        uint32_t h = half_events, f = full_events;
        int n = snprintf(msg, sizeof msg, "CH0=%4lu CH1=%4lu  (half %lu/s, full %lu/s)\r\n",
                         (unsigned long)avg_ch[0], (unsigned long)avg_ch[1],
                         (unsigned long)(h - last_h), (unsigned long)(f - last_f));
        last_h = h; last_f = f;
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);   /* 기대값: 각각 약 31 */
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님, HAL_DMA_IRQHandler 가 호출)                              */
/* ------------------------------------------------------------------------- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)   /* 앞 절반이 채워짐 (DMA 는 뒤 절반에 쓰는 중) */
{
    if (hadc->Instance == ADC1) { process_half(&adc_buf[0]); half_events++; }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)       /* 뒤 절반이 채워짐 (DMA 는 앞 절반으로 되돌아감) */
{
    if (hadc->Instance == ADC1) { process_half(&adc_buf[BUF_LEN / 2]); full_events++; }
}

/* ---- MSP ---- */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();     /* RCC->APB2ENR.ADC1EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    g.Mode = GPIO_MODE_ANALOG;       /* CNF=00, MODE=00 아날로그 입력 */
    HAL_GPIO_Init(GPIOA, &g);

    hdma_adc1.Instance = DMA1_Channel1;                         /* ADC1 은 DMA1 채널1 고정 */
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;   /* ADC 결과 12비트 -> 16비트 */
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_MEDIUM;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();    /* RCC->APB1ENR.TIM3EN */
}

/* TIM3: 64MHz / 64 = 1MHz, ARR = 1000-1 -> 1kHz 업데이트, 업데이트 이벤트를 TRGO 로 출력 */
static void MX_TIM3_Init(void)
{
    TIM_MasterConfigTypeDef m = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 64 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();

    m.MasterOutputTrigger = TIM_TRGO_UPDATE;                    /* CR2.MMS = 010 */
    m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &m) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};

    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;                   /* 64/6 = 10.67MHz */
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;                  /* CR1.SCAN */
    hadc1.Init.ContinuousConvMode = DISABLE;                    /* 트리거 1번 = 스캔 1회 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO; /* CR2.EXTSEL = 100 */
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = N_CH;                          /* SQR1.L = 1 */
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    ch.Channel = ADC_CHANNEL_0;   ch.Rank = ADC_REGULAR_RANK_1;  ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    ch.Channel = ADC_CHANNEL_1;   ch.Rank = ADC_REGULAR_RANK_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
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

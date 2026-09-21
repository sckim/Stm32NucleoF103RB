/**
 * 05_ADC_Injected_HAL_c  —  [계층: HAL]  ADC 주입(Injected) 채널: 연속 변환 중에도 우선 끼어드는 변환
 *
 * 정규(regular) 채널과 주입(injected) 채널의 차이 (RM0008 11장)
 *   정규 채널  : 최대 16개 시퀀스. 결과가 하나의 DR(16비트)에 쌓여 DMA 나 빠른 읽기가 필요하다.
 *   주입 채널  : 최대 4개. 결과가 채널별 전용 JDR1~JDR4 에 저장되어 덮어써지지 않고, 정규 변환을 "중단시키고" 먼저 실행된다.
 *                -> 느린 정규 스캔(예: 센서 여러 개)이 도는 중에도 급한 측정(예: 과전류 감시, 모터 전류 샘플링)을 제때 할 수 있다.
 *
 * 이 예제
 *   정규 채널 : ADC1 CH1 (PA1) 를 연속 변환(DMA 로 최신값을 변수에 저장). PA1 에 가변저항 등을 연결.
 *   주입 채널 : ADC1 CH16 (내부 온도 센서)를 500ms 마다 소프트웨어 트리거로 1회 변환, 변환 완료 인터럽트(JEOC)에서 결과 읽기.
 *   -> 정규 변환이 계속 돌고 있어도 온도 측정이 정확히 500ms 마다 실행되는 것을 확인.
 *   온도 계산 (RM0008 11.10, 데이터시트): T[°C] = (V25 - Vsense)/Avg_Slope + 25,  V25 ≈ 1.43V, Avg_Slope ≈ 4.3mV/°C (개체 편차 큼, 절대값은 ±수 °C)
 *
 * 하드웨어 대응
 *   ADC1->JSQR : 주입 시퀀스(JL 길이, JSQx 채널),  CR2.JEXTSEL=111 + JEXTTRIG : 소프트웨어 트리거 (JSWSTART 로 시작)
 *   ADC1->JOFR1 : 주입 채널 오프셋(결과에서 자동 차감),  ADC1->JDR1 : 결과,  SR.JEOC / CR1.JEOCIE : 주입 변환 완료 플래그/인터럽트
 *   CR2.TSVREFE : 온도센서/Vrefint 활성화 (HAL 이 채널 16 설정 시 켬),  샘플 시간 239.5 사이클(온도센서 최소 17.1us 요구)
 *
 * ISR vs 콜백
 *   ADC1_2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_ADC_IRQHandler()
 *     -> HAL_ADCEx_InjectedConvCpltCallback() (이 파일) : 주입 변환 1회 완료
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart2;

static volatile uint16_t reg_val;             /* 정규 채널(PA1) 최신값 (DMA 가 계속 덮어씀) */
static volatile uint32_t temp_raw;            /* 주입 채널(온도센서) 결과 */
static volatile uint32_t inj_count;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    HAL_ADCEx_Calibration_Start(&hadc1);

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&reg_val, 1) != HAL_OK) Error_Handler();   /* 정규 연속 변환 시작 */

    char msg[96];
    uint32_t t_inj = HAL_GetTick();
    while (1)
    {
        if (HAL_GetTick() - t_inj >= 500)
        {
            t_inj += 500;
            if (HAL_ADCEx_InjectedStart_IT(&hadc1) != HAL_OK) Error_Handler();   /* JSWSTART: 정규 변환을 끊고 주입 변환 실행 */
        }

        static uint32_t last_count;
        if (inj_count != last_count)
        {
            last_count = inj_count;
            /* mV = raw x 3300 / 4095 (VDDA=3.3V 가정),  T = (1430 - mV) x 10 / 43 + 25 */
            int32_t mv = (int32_t)(temp_raw * 3300UL / 4095UL);
            int32_t t10 = (1430 - mv) * 100 / 43 + 250;            /* 0.1 °C 단위 */
            int n = snprintf(msg, sizeof msg, "regular(PA1)=%4u  injected(temp raw)=%4lu (%ld mV)  temp=%ld.%ld C  #%lu\r\n",
                             reg_val, (unsigned long)temp_raw, (long)mv, (long)(t10 / 10), (long)(t10 < 0 ? -t10 % 10 : t10 % 10),
                             (unsigned long)inj_count);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 주입 변환 완료 (SR.JEOC)                                  */
/* ------------------------------------------------------------------------- */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    temp_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);   /* ADC1->JDR1 (오프셋 차감 후) */
    inj_count++;
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    g.Pin = GPIO_PIN_1;  g.Mode = GPIO_MODE_ANALOG;        /* PA1 = ADC1_IN1 */
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

    HAL_NVIC_SetPriority(ADC1_2_IRQn, 1, 0);                /* 주입 변환 완료 인터럽트 */
    HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    ADC_InjectionConfTypeDef inj = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};

    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;               /* 10.67MHz */
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;                 /* 정규: 연속 변환 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    ch.Channel = ADC_CHANNEL_1;                             /* 정규 채널 */
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();

    inj.InjectedChannel = ADC_CHANNEL_TEMPSENSOR;           /* 주입 채널 = 온도 센서 (채널 16) */
    inj.InjectedRank = ADC_INJECTED_RANK_1;
    inj.InjectedNbrOfConversion = 1;                        /* JSQR.JL = 0 */
    inj.InjectedSamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    inj.ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START;
    inj.AutoInjectedConv = DISABLE;                         /* CR1.JAUTO = 0 (정규 뒤에 자동 실행하지 않음) */
    inj.InjectedDiscontinuousConvMode = DISABLE;
    inj.InjectedOffset = 0;                                 /* JOFR1 */
    if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &inj) != HAL_OK) Error_Handler();
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

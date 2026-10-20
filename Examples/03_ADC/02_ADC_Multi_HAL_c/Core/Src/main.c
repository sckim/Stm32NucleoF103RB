/**
 * 02_ADC_Multi_HAL_c  —  [계층: HAL]  DMA 없이 여러 채널 읽기: 스캔 모드 + 불연속(discontinuous) 모드 + 폴링
 *
 * 문제: ADC1 레귤러 그룹의 결과 레지스터(DR)는 하나뿐이다. 스캔 모드로 4채널을 한 번에 변환하면
 *       채널마다 DR 이 덮어써지고, EOC 는 시퀀스 끝에서만 서므로 CPU 가 중간 값을 놓친다.
 *       (그래서 보통 DMA 를 쓴다 -> 03_ADC_DMA_TimTrigger_HAL_c)
 * 해법: 불연속 모드(DISCNUM = 1)를 켜면 트리거 1번에 시퀀스의 "다음 1채널"만 변환한다.
 *       Start -> 변환 완료 대기 -> DR 읽기 를 채널 수만큼 반복하면 순서대로 모든 채널을 얻는다.
 *       마지막 랭크까지 변환하면 다음 트리거는 다시 랭크 1부터 시작한다 (RM0008 11.3.10).
 *
 * 변환 시퀀스 (SQR3)
 *   랭크1 CH0  (PA0, Arduino A0)   샘플 55.5 사이클
 *   랭크2 CH1  (PA1, Arduino A1)   샘플 55.5 사이클
 *   랭크3 CH4  (PA4, Arduino A2)   샘플 55.5 사이클
 *   랭크4 CH17 (Vrefint, 내부)     샘플 239.5 사이클 (내부 소스 임피던스가 높음, 최소 17.1us)
 *
 * 동작
 *   - 4채널 스캔을 N_AVG 회 반복해 채널별 평균
 *   - Vrefint 로 실제 VDDA 를 역산하고 (05_ADC_Vrefint_VDDA_HAL_c 참고) 외부 채널을 mV 로 환산
 *   - 1초마다 UART(115200) 출력
 *
 * 하드웨어 대응 (RM0008)
 *   ADC1->CR1.SCAN = 1                 : 스캔 모드 (SQR 시퀀스를 순서대로)
 *   ADC1->CR1.DISCEN = 1, DISCNUM = 000 : 불연속 모드, 트리거당 1채널
 *   ADC1->CR2: EXTSEL = 111 (SWSTART), EXTTRIG = 1, CONT = 0, DMA = 0
 *   ADC1->SQR1.L = 3 (변환 4개), SQR3 = {CH0, CH1, CH4, CH17}
 *   ADC1->SMPR2 (CH0/1/4), SMPR1 (CH17), ADC1->CR2.TSVREFE (HAL 이 CH17 설정 시 자동으로 켬)
 *   ADC 클럭 = PCLK2(64MHz)/6 = 10.67MHz -> 외부 채널 (55.5+12.5)=68 사이클 = 6.4us, Vrefint 252 사이클 = 23.6us
 *
 * HAL 주의점: 스캔 모드(SQR1.L >= 1)에서는 EOC 가 채널마다 서지 않으므로 HAL_ADC_PollForConversion() 이
 *   EOC 를 기다리지 않고 "최대 변환 시간"만큼 소프트웨어 지연 후 반환한다 (stm32f1xx_hal_adc.c 주석 참고).
 *
 * 인터럽트를 사용하지 않는다 (폴링 변환). ISR 은 SysTick_Handler(stm32f1xx_it.c) 뿐이다.
 */
#include "main.h"
#include <stdio.h>

#define N_CH       4U
#define N_AVG      8U
#define VREFINT_MV 1200UL          /* 데이터시트 전형값 (개체 편차 1.16~1.24V) */

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;

/* 랭크 순서와 같은 순서로 채널을 정의. MX_ADC1_Init() 이 이 표로 SQR/SMPR 을 설정한다 */
static const struct { uint32_t channel; uint32_t sample; } seq[N_CH] = {
    { ADC_CHANNEL_0,       ADC_SAMPLETIME_55CYCLES_5  },   /* 랭크1 PA0 */
    { ADC_CHANNEL_1,       ADC_SAMPLETIME_55CYCLES_5  },   /* 랭크2 PA1 */
    { ADC_CHANNEL_4,       ADC_SAMPLETIME_55CYCLES_5  },   /* 랭크3 PA4 */
    { ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_239CYCLES_5 },   /* 랭크4 CH17 */
};

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);

/* 시퀀스 1회(랭크1~4)를 채널 하나씩 변환해 out[] 에 저장 */
static void adc_scan_once(uint16_t out[N_CH])
{
    for (uint32_t r = 0; r < N_CH; r++)
    {
        HAL_ADC_Start(&hadc1);                                    /* CR2.SWSTART -> 다음 랭크 1채널 변환 */
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) Error_Handler();   /* 스캔 모드: 변환 시간만큼 대기 */
        out[r] = (uint16_t)HAL_ADC_GetValue(&hadc1);              /* DR 읽기 (다음 변환 전에 읽어야 함) */
    }
    /* HAL_ADC_Stop() 은 호출하지 않는다: ADON 을 유지한 채 다음 Start 가 랭크1부터 이어진다 */
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();   /* ADC1->CR2.CAL */

    char msg[128];
    while (1)
    {
        uint32_t sum[N_CH] = {0};
        uint16_t raw[N_CH];
        for (uint32_t i = 0; i < N_AVG; i++)
        {
            adc_scan_once(raw);
            for (uint32_t c = 0; c < N_CH; c++) sum[c] += raw[c];
        }
        uint32_t avg[N_CH];
        for (uint32_t c = 0; c < N_CH; c++) avg[c] = sum[c] / N_AVG;

        /* VDDA = Vrefint x 4095 / raw_vref,  V_ch = raw x VDDA / 4095 */
        uint32_t vdda_mv = (avg[3] != 0) ? VREFINT_MV * 4095UL / avg[3] : 0;
        int n = snprintf(msg, sizeof msg,
                         "A0=%4lu(%4lumV) A1=%4lu(%4lumV) A2=%4lu(%4lumV) | Vref=%4lu -> VDDA=%4lumV\r\n",
                         (unsigned long)avg[0], (unsigned long)(avg[0] * vdda_mv / 4095UL),
                         (unsigned long)avg[1], (unsigned long)(avg[1] * vdda_mv / 4095UL),
                         (unsigned long)avg[2], (unsigned long)(avg[2] * vdda_mv / 4095UL),
                         (unsigned long)avg[3], (unsigned long)vdda_mv);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(1000);
    }
}

/* ---- MSP ---- */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();     /* RCC->APB2ENR.ADC1EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
    g.Mode = GPIO_MODE_ANALOG;       /* CRL: CNF=00, MODE=00 아날로그 입력 */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};

    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;                     /* RCC->CFGR.ADCPRE: 64/6 = 10.67MHz (최대 14MHz) */
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;                    /* CR1.SCAN = 1 */
    hadc1.Init.ContinuousConvMode = DISABLE;                      /* CR2.CONT = 0 */
    hadc1.Init.DiscontinuousConvMode = ENABLE;                    /* CR1.DISCEN = 1 */
    hadc1.Init.NbrOfDiscConversion = 1;                           /* CR1.DISCNUM = 000 (트리거당 1채널) */
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;             /* CR2.EXTSEL = 111 (SWSTART) */
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;                   /* CR2.ALIGN = 0 */
    hadc1.Init.NbrOfConversion = N_CH;                            /* SQR1.L = N_CH-1 */
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    for (uint32_t r = 0; r < N_CH; r++)
    {
        ch.Channel = seq[r].channel;
        ch.Rank = ADC_REGULAR_RANK_1 + r;                         /* SQR3.SQ1~SQ4 */
        ch.SamplingTime = seq[r].sample;                          /* SMPR1/SMPR2.SMPx */
        if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    }
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

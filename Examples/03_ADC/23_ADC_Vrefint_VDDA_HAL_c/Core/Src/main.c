/**
 * 23_ADC_Vrefint_VDDA_HAL_c  —  [계층: HAL]  내부 기준전압(Vrefint)으로 실제 VDDA 를 측정하고 ADC 값을 보정
 *
 * 문제: ADC 는 "VDDA(기준전압) 대비 비율"을 변환한다. mV = raw x 3300 / 4095 는 VDDA 가 정확히 3.3V 일 때만 맞다.
 *       USB/배터리 전원에서 VDDA 가 3.0~3.4V 로 변하면 오차가 생긴다.
 * 해법: 내부 기준전압 Vrefint(약 1.20V, 온도/전원에 거의 무관)를 ADC 로 측정하면 VDDA 를 역산할 수 있다.
 *       raw_vref = Vrefint x 4095 / VDDA   ->   VDDA = Vrefint x 4095 / raw_vref
 *       (Vrefint 는 전형값 1.20V, 개체 편차 1.16~1.24V (데이터시트). F1 은 STM32 후속 시리즈와 달리 개별 공장 교정값이 없다)
 *
 * 동작
 *   - ADC1 채널 17(Vrefint, 내부) 을 16회 평균 -> VDDA 계산
 *   - ADC1 채널 0(PA0) 도 16회 평균 -> "측정 VDDA 기준 mV" 와 "3.3V 가정 mV" 를 함께 출력해 차이를 비교
 *   - 1초마다 UART(115200) 출력
 *
 * 하드웨어 대응
 *   ADC1->CR2.TSVREFE(bit23) : 온도센서/Vrefint 활성화 (HAL 이 채널 16/17 설정 시 자동으로 켬)
 *   Vrefint 채널은 샘플 시간을 길게(239.5 사이클, 내부 소스 임피던스가 높음) 잡아야 정확하다 (데이터시트: 최소 17.1us)
 *   ADC 클럭 10.67MHz -> 239.5+12.5 = 252 사이클 = 23.6us  (요구치 만족)
 *
 * 인터럽트를 사용하지 않는다 (폴링 변환).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define VREFINT_MV 1200UL          /* 전형값. 정밀도가 필요하면 개체별로 측정/보정 */
#define N_AVG      16U

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);

/* 지정 채널을 N_AVG 번 폴링 변환해 평균 */
static uint32_t adc_read_avg(uint32_t channel)
{
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = channel;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();

    uint32_t sum = 0;
    for (uint32_t i = 0; i < N_AVG; i++)
    {
        HAL_ADC_Start(&hadc1);                                    /* CR2.ADON / SWSTART */
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) Error_Handler();   /* SR.EOC */
        sum += HAL_ADC_GetValue(&hadc1);                          /* DR 읽기 */
    }
    HAL_ADC_Stop(&hadc1);
    return sum / N_AVG;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    HAL_ADCEx_Calibration_Start(&hadc1);

    char msg[112];
    while (1)
    {
        uint32_t raw_vref = adc_read_avg(ADC_CHANNEL_VREFINT);
        uint32_t raw_a0   = adc_read_avg(ADC_CHANNEL_0);

        uint32_t vdda_mv = (raw_vref != 0) ? VREFINT_MV * 4095UL / raw_vref : 0;   /* VDDA = Vrefint x 4095 / raw */
        uint32_t a0_naive = raw_a0 * 3300UL / 4095UL;                              /* VDDA=3.3V 가정 */
        uint32_t a0_corr  = raw_a0 * vdda_mv / 4095UL;                             /* 실측 VDDA 기준 */

        int n = snprintf(msg, sizeof msg, "raw_vref=%4lu -> VDDA=%4lu mV | A0 raw=%4lu : assume3.3V=%4lu mV, corrected=%4lu mV\r\n",
                         (unsigned long)raw_vref, (unsigned long)vdda_mv, (unsigned long)raw_a0,
                         (unsigned long)a0_naive, (unsigned long)a0_corr);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(1000);
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_ADC1_Init(void)
{
    RCC_PeriphCLKInitTypeDef pc = {0};
    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;                     /* 10.67MHz (최대 14MHz) */
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
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

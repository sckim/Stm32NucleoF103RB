/**
 * 22_ADC_AnalogWatchdog_HAL_c  —  [계층: HAL]  ADC 아날로그 워치독(AWD): 전압이 창(window)을 벗어나면 하드웨어가 인터럽트
 *
 * 개념: ADC 가 변환을 끝낼 때마다 "결과 > 상한 또는 결과 < 하한"인지 하드웨어가 비교하고, 벗어나면 플래그/인터럽트를 낸다.
 *       CPU 가 값을 계속 읽어 비교하지 않아도 되므로 과전압/저전압/과전류 보호에 적합하다.
 *
 * 동작
 *   - PA0(A0) 의 전압을 연속 변환(DMA 로 최신값을 변수에 자동 저장). 창: 1000 ~ 3000 (약 0.81V ~ 2.42V @3.3V)
 *   - 창을 벗어나면 AWD 인터럽트 -> 콜백이 LD2 를 켜고 플래그 설정. 창 안으로 돌아오면 main 이 LD2 를 끄고 AWD 를 다시 무장.
 *   - 200ms 마다 현재 값과 상태를 UART(115200)로 출력.  가변저항으로 PA0 를 0~3.3V 로 돌리며 확인.
 *
 * 하드웨어 대응 (RM0008 11.3.7 Analog watchdog)
 *   ADC1->HTR / LTR : 상한/하한 임계값(12비트)
 *   ADC1->CR1.AWDEN(bit23) : 정규 채널 AWD 허용,  CR1.AWDSGL(bit9) : 단일 채널만 감시,  CR1.AWDCH : 감시 채널,  CR1.AWDIE(bit6) : AWD 인터럽트
 *   ADC1->SR.AWD(bit0) : 창을 벗어났음 플래그
 *   *** 창을 벗어난 상태에서는 "매 변환마다" AWD 가 다시 발생해 인터럽트가 폭주한다(초당 수만 회).
 *       그래서 콜백에서 AWD 인터럽트를 끄고, main 이 창 안으로 돌아온 것을 확인한 뒤 다시 켠다. ***
 *   ADC 클럭 = 64MHz/6 = 10.67MHz, 샘플 239.5 사이클 -> 약 42kSPS
 *
 * ISR vs 콜백
 *   ADC1_2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_ADC_IRQHandler() -> HAL_ADC_LevelOutOfWindowCallback() (이 파일)
 *   (DMA 전송 완료 인터럽트는 NVIC 에서 허용하지 않아 ISR 부하가 없다)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define AWD_HIGH 3000U
#define AWD_LOW  1000U

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart2;

static volatile uint16_t adc_val;          /* DMA 가 계속 갱신하는 최신 변환값 */
static volatile uint8_t  awd_flag;
static volatile uint32_t awd_events;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();

    HAL_ADCEx_Calibration_Start(&hadc1);
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_val, 1) != HAL_OK) Error_Handler();   /* 1워드 원형 DMA: 항상 최신값 */

    char msg[80];
    while (1)
    {
        HAL_Delay(200);
        uint16_t v = adc_val;
        int in_window = (v >= AWD_LOW && v <= AWD_HIGH);

        if (in_window && awd_flag)                       /* 창 안으로 복귀 -> 재무장 */
        {
            awd_flag = 0;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD);
            __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_AWD);     /* CR1.AWDIE = 1 */
        }

        int n = snprintf(msg, sizeof msg, "ADC=%4u (%4lu mV)  window[%u..%u]  %s  events=%lu\r\n",
                         v, (unsigned long)v * 3300UL / 4095UL, AWD_LOW, AWD_HIGH,
                         awd_flag ? "OUT-OF-WINDOW" : "ok", (unsigned long)awd_events);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 변환값이 창을 벗어났을 때 (SR.AWD)                        */
/* ------------------------------------------------------------------------- */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    awd_flag = 1;
    awd_events++;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   /* 즉시 경고 표시 */
    __HAL_ADC_DISABLE_IT(hadc, ADC_IT_AWD);               /* 인터럽트 폭주 방지: main 이 복귀를 확인한 뒤 재활성화 */
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &g);

    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_DISABLE;                     /* 항상 같은 변수에 덮어씀 */
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(ADC1_2_IRQn, 1, 0);                      /* AWD 인터럽트 */
    HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};
    ADC_AnalogWDGConfTypeDef awd = {0};
    RCC_PeriphCLKInitTypeDef pc = {0};

    pc.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    pc.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;                        /* CR2.CONT = 1 : 연속 변환 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    ch.Channel = ADC_CHANNEL_0;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();

    awd.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;             /* CR1.AWDEN=1, AWDSGL=1 : 정규 채널 하나만 감시 */
    awd.Channel = ADC_CHANNEL_0;                                  /* CR1.AWDCH */
    awd.HighThreshold = AWD_HIGH;                                 /* ADC1->HTR */
    awd.LowThreshold = AWD_LOW;                                   /* ADC1->LTR */
    awd.ITMode = ENABLE;                                          /* CR1.AWDIE = 1 */
    if (HAL_ADC_AnalogWDGConfig(&hadc1, &awd) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;   /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
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

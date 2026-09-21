/**
 * 10_TIM_Cascade32_HAL_c  —  [계층: HAL]  타이머 마스터/슬레이브 연결로 만든 32비트 카운터 (스톱워치 71분)
 *
 * F103 의 범용 타이머는 16비트라 1MHz(1us)로 세면 65.5ms 마다 넘친다. 타이머 두 개를 **연결**(cascade)하면 32비트가 된다.
 *
 * 구조 (RM0008 15.3.15 Timer synchronization)
 *   TIM3 = 마스터: PSC = 64-1 (1MHz), ARR = 0xFFFF. 넘칠 때(업데이트 이벤트) **TRGO 로 트리거 출력** (CR2.MMS = 010)
 *   TIM2 = 슬레이브: **외부 클럭 모드 1**(SMCR.SMS = 111)에서 트리거 입력 **ITR2**(= TIM3 의 TRGO, SMCR.TS = 010)를 클럭으로 사용
 *          -> TIM3 가 65536 번 세고 넘칠 때마다 TIM2 가 1 증가한다.
 *   32비트 값 = (TIM2->CNT << 16) | TIM3->CNT  -> 1us 분해능으로 최대 2^32 us = 약 71.6분.
 *   (TIM2 의 ITRx 매핑은 마스터 타이머에 따라 다르다: ITR0=TIM1, ITR1=TIM8(없음), ITR2=TIM3, ITR3=TIM4. RM0008 표 참고)
 *
 * 읽기 경합 주의: 두 카운터를 순서대로 읽는 사이 TIM3 가 넘치면 값이 튈 수 있다.
 *   -> 상위(TIM2)-하위(TIM3)-상위(TIM2) 순으로 읽어 상위가 바뀌었으면 다시 읽는다.
 *
 * 동작
 *   - 1초마다 32비트 시각(us)과 HAL_GetTick(ms)을 비교 출력 -> 거의 1,000,000us 씩 증가하는지 확인 (같은 64MHz 클럭 기반이라 오차 작음)
 *   - B1(PC13): 스톱워치. 첫 번째 누름 = 시작, 두 번째 누름 = 정지하고 경과 시간(us -> h:m:s.ms)을 출력, 세 번째 = 다시 시작
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

TIM_HandleTypeDef htim2;        /* 슬레이브: 상위 16비트 */
TIM_HandleTypeDef htim3;        /* 마스터: 하위 16비트 */
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM_Cascade_Init(void);

/* 32비트 마이크로초 카운터를 경합 없이 읽는다 */
static uint32_t micros32(void)
{
    uint32_t hi1, lo, hi2;
    do {
        hi1 = TIM2->CNT;
        lo  = TIM3->CNT;
        hi2 = TIM2->CNT;
    } while (hi1 != hi2);                   /* 읽는 도중 하위가 넘쳐 상위가 바뀌었으면 재시도 */
    return (hi2 << 16) | lo;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM_Cascade_Init();

    char msg[96];
    uint32_t t_us0 = micros32(), t_ms0 = HAL_GetTick();
    uint32_t t_print = HAL_GetTick();
    int running = 0;
    uint32_t sw_start = 0;

    while (1)
    {
        if (HAL_GetTick() - t_print >= 1000)
        {
            t_print += 1000;
            uint32_t us = micros32(), ms = HAL_GetTick();
            int n = snprintf(msg, sizeof msg, "micros32=%10lu (d=%lu us)  HAL tick=%lu ms (d=%lu ms)\r\n",
                             (unsigned long)us, (unsigned long)(us - t_us0), (unsigned long)ms, (unsigned long)(ms - t_ms0));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
            t_us0 = us; t_ms0 = ms;
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1: 스톱워치 시작/정지 */
        {
            HAL_Delay(30);
            if (!running)
            {
                sw_start = micros32();
                running = 1;
                HAL_UART_Transmit(&huart2, (uint8_t *)"stopwatch: START\r\n", 18, 100);
            }
            else
            {
                uint32_t el = micros32() - sw_start;                  /* 부호 없는 뺄셈: 71분 래핑에도 안전 */
                running = 0;
                uint32_t ms = el / 1000UL, s = ms / 1000UL, m = s / 60UL;
                int n = snprintf(msg, sizeof msg, "stopwatch: STOP  %lu us  = %lu:%02lu.%03lu (m:s.ms)\r\n",
                                 (unsigned long)el, (unsigned long)m, (unsigned long)(s % 60UL), (unsigned long)(ms % 1000UL));
                HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
            }
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(30);
        }
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
}

static void MX_TIM_Cascade_Init(void)
{
    TIM_MasterConfigTypeDef m = {0};
    TIM_SlaveConfigTypeDef s = {0};

    /* 슬레이브 TIM2: 자체 분주 없음, 최대 카운트. 클럭은 TIM3 넘침(ITR2)에서 온다 */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
    s.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;              /* SMCR.SMS = 111 : 외부 클럭 모드 1 */
    s.InputTrigger = TIM_TS_ITR2;                       /* SMCR.TS = 010 : 트리거 = TIM3 TRGO */
    if (HAL_TIM_SlaveConfigSynchro(&htim2, &s) != HAL_OK) Error_Handler();

    /* 마스터 TIM3: 1MHz, 넘칠 때(update) TRGO 출력 */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 64 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFF;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();
    m.MasterOutputTrigger = TIM_TRGO_UPDATE;            /* CR2.MMS = 010 */
    m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &m) != HAL_OK) Error_Handler();

    /* 슬레이브를 먼저 켜고(대기), 마스터를 켠다 */
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK) Error_Handler();
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;    /* B1 */
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

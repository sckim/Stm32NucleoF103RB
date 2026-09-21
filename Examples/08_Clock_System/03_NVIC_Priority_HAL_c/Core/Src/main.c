/**
 * 03_NVIC_Priority_HAL_c  —  [계층: HAL]  NVIC 우선순위 그룹(선점/서브)과 인터럽트 중첩 실험
 *
 * 실험 구성
 *   TIM3 ISR : 10ms 마다 발생, 짧다. 진입 간격의 "최대값(지터)"을 측정한다. (급한 인터럽트 역할)
 *   TIM2 ISR : 100ms 마다 발생, 일부러 20ms 동안 바쁜 대기한다. (오래 걸리는 인터럽트 역할)
 *   -> TIM2 ISR 이 실행되는 20ms 동안 TIM3 가 제때 들어올 수 있는지가 우선순위 설정에 달렸다.
 *
 * B1(PC13)을 누를 때마다 우선순위 케이스가 A -> B -> C 로 바뀌고, 1초마다 결과를 UART(115200)로 출력한다.
 *
 *   케이스 A (TIM3 선점 우선순위 0 < TIM2 1) : TIM3 가 TIM2 ISR 을 "선점(중첩)". 최대 간격 ~10ms 유지.
 *   케이스 B (선점 우선순위 같음 1, 서브만 TIM3 가 높음) : 서브 우선순위는 "선점하지 못한다".
 *                                              TIM3 는 TIM2 ISR 이 끝날 때까지 대기 -> 최대 간격 ~20~30ms.
 *   케이스 C (TIM3 선점 2, TIM2 선점 1) : 긴 ISR 에 더 높은 우선순위를 준 잘못된 배정. B 와 비슷하게 지연.
 *   교훈: "짧고 급한" 인터럽트에 높은 우선순위를, "길고 덜 급한" 인터럽트에 낮은 우선순위를 주어야 한다.
 *
 * NVIC 우선순위 (STM32F103: 우선순위 비트 4개 = 상위 4비트만 구현)
 *   HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2) -> SCB->AIRCR.PRIGROUP = 5
 *     -> 우선순위 필드 [7:4] 중 상위 2비트 = 선점(preempt, 0~3), 하위 2비트 = 서브(sub, 0~3)
 *   HAL_NVIC_SetPriority(irq, preempt, sub) -> NVIC->IP[irq] 에 인코딩하여 기록. 숫자가 작을수록 우선순위가 높다.
 *   같은 선점 우선순위끼리는 중첩 불가, 동시에 대기 중일 때만 서브 우선순위로 실행 순서를 정한다.
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler / TIM3_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler()
 *     -> HAL_TIM_PeriodElapsedCallback() (이 파일) : 두 타이머가 같은 콜백을 공유하므로 Instance 로 구분
 *   ISR 안에서 UART 출력을 하지 않고 통계 변수만 갱신한다 (출력은 main).
 *
 * 측정: DWT 사이클 카운터 (64MHz). SysTick 은 TICK_INT_PRIORITY=0(최고)이라 HAL_Delay 가 흔들리지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define CPU_MHZ 64U

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

static volatile uint32_t tim3_last;         /* TIM3 직전 진입 시각 (사이클) */
static volatile uint32_t tim3_max_gap_us;   /* 최근 1초간 TIM3 진입 간격 최대값 [us] */
static volatile uint32_t tim3_count, tim2_count;
static volatile uint8_t  tim3_first = 1;

static uint8_t prio_case;                   /* 0=A, 1=B, 2=C */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM_Init(void);
static void apply_case(uint8_t c);

static const char *const case_desc[3] = {
    "A: TIM3(pre 0) preempts TIM2(pre 1)  -> nesting",
    "B: same preempt(1), sub only         -> no nesting",
    "C: TIM3(pre 2) below TIM2(pre 1)     -> long ISR wins"
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* DWT 사이클 카운터 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);   /* SCB->AIRCR.PRIGROUP : 선점 2비트 / 서브 2비트 */
    MX_TIM_Init();
    apply_case(0);

    char msg[112];
    uint32_t t_print = DWT->CYCCNT;
    while (1)
    {
        /* 1초마다 통계 출력 (SysTick 이 아니라 DWT 로 시간을 잰다) */
        if ((DWT->CYCCNT - t_print) >= CPU_MHZ * 1000000U)
        {
            t_print += CPU_MHZ * 1000000U;

            __disable_irq();                               /* ISR 이 쓰는 변수를 한꺼번에 읽고 초기화 */
            uint32_t gap = tim3_max_gap_us, c3 = tim3_count, c2 = tim2_count;
            tim3_max_gap_us = 0; tim3_count = 0; tim2_count = 0;
            __enable_irq();

            int n = snprintf(msg, sizeof msg, "[%s] TIM3 max gap=%lu us, TIM3=%lu, TIM2=%lu (1s)\r\n",
                             case_desc[prio_case], (unsigned long)gap, (unsigned long)c3, (unsigned long)c2);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1 : 케이스 전환 */
        {
            HAL_Delay(30);
            apply_case((uint8_t)((prio_case + 1) % 3));
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(30);
        }
    }
}

/* 케이스별 NVIC 우선순위 적용. NVIC->IP[TIM2_IRQn / TIM3_IRQn] 를 다시 쓴다 */
static void apply_case(uint8_t c)
{
    static const uint8_t tim3_pre[3] = {0, 1, 2}, tim3_sub[3] = {0, 0, 0};
    static const uint8_t tim2_pre[3] = {1, 1, 1}, tim2_sub[3] = {0, 1, 0};

    HAL_NVIC_SetPriority(TIM3_IRQn, tim3_pre[c], tim3_sub[c]);
    HAL_NVIC_SetPriority(TIM2_IRQn, tim2_pre[c], tim2_sub[c]);

    __disable_irq();
    prio_case = c;
    tim3_first = 1; tim3_max_gap_us = 0; tim3_count = 0; tim2_count = 0;
    __enable_irq();
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 두 타이머의 주기 인터럽트가 같은 콜백으로 들어온다              */
/* ------------------------------------------------------------------------- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)                            /* 짧고 급한 인터럽트: 진입 간격 측정 */
    {
        uint32_t now = DWT->CYCCNT;
        if (!tim3_first)
        {
            uint32_t gap_us = (now - tim3_last) / CPU_MHZ;
            if (gap_us > tim3_max_gap_us) tim3_max_gap_us = gap_us;
        }
        tim3_first = 0;
        tim3_last = now;
        tim3_count++;
    }
    else if (htim->Instance == TIM2)                       /* 길고 덜 급한 인터럽트: 20ms 점유 */
    {
        uint32_t t = DWT->CYCCNT;
        while ((DWT->CYCCNT - t) < CPU_MHZ * 20000U) {}    /* 20ms 바쁜 대기 (실제 코드에서는 절대 금지) */
        tim2_count++;
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
        HAL_NVIC_EnableIRQ(TIM2_IRQn);                     /* 우선순위는 apply_case 에서 지정 */
    }
    else if (htim->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        HAL_NVIC_EnableIRQ(TIM3_IRQn);
    }
}

/* 타이머 클럭 64MHz / 6400 = 10kHz.  TIM3: ARR 100 -> 10ms,  TIM2: ARR 1000 -> 100ms */
static void MX_TIM_Init(void)
{
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 6400 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 100 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();

    htim2 = htim3;
    htim2.Instance = TIM2;
    htim2.Init.Period = 1000 - 1;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK) Error_Handler();   /* DIER.UIE = 1, CR1.CEN = 1 */
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) Error_Handler();
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

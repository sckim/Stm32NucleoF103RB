/**
 * 04_TIM1_DeadTime_Break_HAL_c  —  [계층: HAL]  고급 타이머 TIM1: 상보 PWM + 데드타임 + 브레이크
 *
 * 모터 구동(하프브리지)에서 상단/하단 스위치가 동시에 켜져 전원이 단락되는 것을 막는 기능들.
 *
 * 출력 (Nucleo-F103RB, 관측은 오실로스코프/로직 분석기)
 *   TIM1_CH1  = PA8  ('D7')          20kHz, 듀티 50%
 *   TIM1_CH1N = PB13 (Morpho CN10)   CH1 의 반전 + 데드타임 삽입
 *   TIM1_BKIN = PB12 (Morpho CN10)   외부 브레이크 입력 (High 가 되면 출력 강제 차단)
 *
 * 타이밍 계산 (TIM1 은 APB2 = 64MHz 에 연결)
 *   PSC = 0, ARR = 3200-1  ->  64MHz / 3200 = 20kHz,  CCR1 = 1600 -> 듀티 50%
 *   데드타임 = DTG x t_DTS,  t_DTS = 1/64MHz = 15.6ns (CR1.CKD = 00)
 *     HAL 의 DeadTime 인자 = BDTR.DTG[7:0] 레지스터 값 그대로
 *     DTG < 128        : DT = DTG x t_DTS            (DTG=64 -> 약 1.0us)
 *     128 <= DTG < 192 : DT = (64 + DTG[5:0]) x 2 x t_DTS  (더 긴 데드타임을 성긴 단위로)
 *   -> 이 예제는 DeadTime = 64 (약 1us): CH1 이 꺼진 뒤 1us 후에 CH1N 이 켜지고, 반대도 동일.
 *
 * 브레이크 동작 (TIM1->BDTR)
 *   BKE=1(브레이크 입력 사용), BKP=1(High 활성), MOE 가 하드웨어에 의해 0 이 되어 출력 전부 OFF,
 *   SR.BIF=1 + 브레이크 인터럽트. AOE=0 이므로 브레이크 해제 후에도 소프트웨어가 MOE 를 다시 켜야 한다.
 *
 * 시험 방법
 *   1) 스코프로 PA8 과 PB13 을 동시에 보면 두 신호가 겹치지 않고 사이에 1us 공백이 있다.
 *   2) B1(PC13) 을 누르면 소프트웨어 브레이크 발생(TIM1->EGR.BG). 두 출력이 즉시 꺼지고 LD2 가 켜지며,
 *      2초 뒤 MOE 를 다시 켜서 PWM 이 재개된다. (외부 신호로 시험하려면 PB12 에 3.3V 를 인가)
 *
 * ISR vs 콜백
 *   TIM1_BRK_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler()
 *     -> HAL_TIMEx_BreakCallback() (이 파일) : 브레이크 발생 통지
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;

static volatile uint8_t break_flag;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM1_Init();

    const char *banner = "\r\n[TIM1 complementary PWM 20kHz, dead-time ~1us] B1: software break\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)banner, (uint16_t)strlen(banner), 100);

    while (1)
    {
        if (break_flag)
        {
            break_flag = 0;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);              /* LD2 ON: 브레이크 상태 표시 */
            const char *m = "BREAK! outputs disabled (MOE=0), re-enable in 2s\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)strlen(m), 100);

            HAL_Delay(2000);

            __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);                    /* SR.BIF = 0 (브레이크 입력이 해제된 후에) */
            __HAL_TIM_MOE_ENABLE(&htim1);                                    /* BDTR.MOE = 1 : 출력 재개 */
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            m = "PWM resumed\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)strlen(m), 100);
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)          /* B1 눌림 */
        {
            HAL_Delay(30);
            htim1.Instance->EGR |= TIM_EGR_BG;                               /* EGR.BG = 1 : 소프트웨어 브레이크 발생 */
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님, HAL_TIM_IRQHandler 가 호출)                              */
/* ------------------------------------------------------------------------- */
void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) break_flag = 1;      /* ISR 에서는 플래그만, 후처리는 main */
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM1) return;
    __HAL_RCC_TIM1_CLK_ENABLE();                     /* RCC->APB2ENR.TIM1EN */
    HAL_NVIC_SetPriority(TIM1_BRK_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM1_BRK_IRQn);
}

static void MX_TIM1_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    TIM_BreakDeadTimeConfigTypeDef bd = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Pin = GPIO_PIN_8;   g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* CH1 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* CH1N */
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_12;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_PULLDOWN;            /* BKIN: 평소 Low 유지 */
    HAL_GPIO_Init(GPIOB, &g);

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 3200 - 1;                    /* 20kHz */
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 1600;                                 /* 50% */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;             /* CCER.CC1P = 0 */
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;           /* CCER.CC1NP = 0 : CH1N = CH1 의 반전(데드타임 포함) */
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;          /* CR2.OIS1 : 브레이크/IDLE 시 출력 상태 */
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;        /* CR2.OIS1N */
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    bd.OffStateRunMode = TIM_OSSR_DISABLE;
    bd.OffStateIDLEMode = TIM_OSSI_DISABLE;
    bd.LockLevel = TIM_LOCKLEVEL_OFF;
    bd.DeadTime = 64;                                /* BDTR.DTG : 64 x 15.6ns = 약 1us */
    bd.BreakState = TIM_BREAK_ENABLE;                /* BDTR.BKE */
    bd.BreakPolarity = TIM_BREAKPOLARITY_HIGH;       /* BDTR.BKP : High 에서 브레이크 */
    bd.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;/* BDTR.AOE = 0 : MOE 를 소프트웨어가 복구 */
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &bd) != HAL_OK) Error_Handler();

    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);       /* DIER.BIE = 1 */

    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) Error_Handler();      /* CCER.CC1E, BDTR.MOE, CR1.CEN */
    if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) Error_Handler();   /* CCER.CC1NE */
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;  /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_INPUT;      g.Pull = GPIO_NOPULL;                                /* B1 */
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

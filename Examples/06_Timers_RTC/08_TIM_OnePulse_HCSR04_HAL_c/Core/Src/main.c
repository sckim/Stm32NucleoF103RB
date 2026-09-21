/**
 * 08_TIM_OnePulse_HCSR04_HAL_c  —  [계층: HAL + 레지스터(OPM 비트)]  원 펄스 모드로 10us 트리거 + 입력 캡처로 초음파 거리 측정
 *
 * HC-SR04 초음파 센서 프로토콜
 *   TRIG 핀에 10us 이상의 High 펄스 -> 센서가 40kHz 초음파 8발 송출 -> ECHO 핀이 "왕복 시간" 동안 High
 *   거리[cm] = ECHO 폭[us] / 58   (음속 343m/s, 왕복이므로 /2)
 *
 * 배선 (Nucleo-F103RB)
 *   VCC = 5V, GND = GND
 *   TRIG = PA6 ('D12', TIM3_CH1)   <- 타이머 원 펄스 출력
 *   ECHO = PA0 ('A0', TIM2_CH1/CH2 입력 캡처)  *** ECHO 는 5V 신호이므로 저항 분압(예: 1kΩ + 2kΩ)으로 3.3V 이하로 낮출 것 ***
 *
 * 원 펄스 모드(OPM) (RM0008 15.3.10)
 *   TIM3->CR1.OPM = 1 : 카운터가 다음 업데이트 이벤트에서 스스로 멈춘다(CEN 자동 클리어).
 *   PWM 모드 2 + OPM: 카운터가 CCR1 이 되기 전에는 Low, 이후 ARR 까지 High -> "CCR1 지연 후 (ARR-CCR1) 폭의 펄스 1개"
 *   1MHz 카운터에서 CCR1 = 2, ARR = 12  ->  2us 지연 후 10us 폭의 High 펄스.  CEN 을 세울 때마다 펄스가 1번 나간다.
 *   -> CPU 가 10us 를 바쁜 대기하지 않고 하드웨어가 정확한 폭을 만든다.
 *
 * 에코 폭 측정: TIM2 의 CH1(상승 에지 캡처)/CH2(하강 에지 캡처, 같은 TI1 입력)  -> 폭 = CCR2 - CCR1
 *   1MHz(1us) 16비트 -> 최대 65ms (약 11m) 까지 측정 가능. HC-SR04 유효 범위 2~400cm(약 23ms) 충분.
 *
 * ISR vs 콜백
 *   TIM2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_TIM_IRQHandler() -> HAL_TIM_IC_CaptureCallback() (이 파일)
 *   (CH2 = 하강 에지 = 에코 종료 시점에 폭이 완성된다)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

TIM_HandleTypeDef htim2;      /* ECHO 캡처 */
TIM_HandleTypeDef htim3;      /* TRIG 원 펄스 */
UART_HandleTypeDef huart2;

static volatile uint32_t echo_us;
static volatile uint8_t echo_ready;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_OnePulse_Init(void);
static void MX_TIM2_Echo_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_TIM3_OnePulse_Init();
    MX_TIM2_Echo_Init();

    char msg[64];
    while (1)
    {
        echo_ready = 0;
        htim3.Instance->CR1 |= TIM_CR1_CEN;            /* 원 펄스 발사: CEN=1 -> 10us High 펄스 1개 후 자동 정지 */

        uint32_t t0 = HAL_GetTick();
        while (!echo_ready && (HAL_GetTick() - t0) < 60) {}       /* 에코 대기 (최대 60ms) */

        int n;
        if (echo_ready)
            n = snprintf(msg, sizeof msg, "echo=%lu us  distance=%lu.%lu cm\r\n", (unsigned long)echo_us,
                         (unsigned long)(echo_us / 58UL), (unsigned long)((echo_us * 10UL / 58UL) % 10UL));
        else
            n = snprintf(msg, sizeof msg, "no echo (check wiring / range)\r\n");
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(100);                                 /* 측정 주기 (HC-SR04 권장 60ms 이상) */
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): CH2 하강 에지 = 에코 종료                                 */
/* ------------------------------------------------------------------------- */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        uint16_t rise = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);   /* CCR1 : 상승 에지 시각 */
        uint16_t fall = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);   /* CCR2 : 하강 에지 시각 */
        echo_us = (uint16_t)(fall - rise);              /* 16비트 래핑에 안전한 차 */
        echo_ready = 1;
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

static void MX_TIM3_OnePulse_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_6;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;      /* TRIG = TIM3_CH1 */
    HAL_GPIO_Init(GPIOA, &g);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 64 - 1;                      /* 1MHz -> 1us */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 12;                             /* ARR = 12 */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM2;                        /* CNT < CCR1 : Low,  이후 High */
    oc.Pulse = 2;                                       /* CCR1 = 2 -> 2us 지연 후 High, ARR(12)까지 = 10us 폭 */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    htim3.Instance->CR1 |= TIM_CR1_OPM;                 /* CR1.OPM = 1 : 업데이트 이벤트에서 카운터 자동 정지 */
    htim3.Instance->CCER |= TIM_CCER_CC1E;              /* CH1 출력 활성 (CEN 은 발사 때 세움) */
}

/* TIM2: 입력 TI1(PA0) 의 상승 에지 -> CCR1, 하강 에지 -> CCR2 (같은 핀을 두 채널이 공유) */
static void MX_TIM2_Echo_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_IC_InitTypeDef ic = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_PULLDOWN;             /* 평소 Low 유지 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;                      /* 1MHz -> 1us */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK) Error_Handler();

    ic.ICPolarity = TIM_ICPOLARITY_RISING;
    ic.ICSelection = TIM_ICSELECTION_DIRECTTI;          /* CH1 <- TI1 */
    ic.ICPrescaler = TIM_ICPSC_DIV1;
    ic.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &ic, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    ic.ICPolarity = TIM_ICPOLARITY_FALLING;
    ic.ICSelection = TIM_ICSELECTION_INDIRECTTI;        /* CH2 <- TI1 (간접) */
    if (HAL_TIM_IC_ConfigChannel(&htim2, &ic, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

    if (HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();      /* CH1 은 값만 캡처 */
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2) != HAL_OK) Error_Handler();   /* CH2 인터럽트: 에코 종료 통지 */
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

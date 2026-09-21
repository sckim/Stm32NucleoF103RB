/**
 * 07_TIM_Encoder_HAL_c  —  [계층: HAL]  타이머 엔코더 인터페이스 모드: 직교(quadrature) 엔코더를 하드웨어로 카운트
 *
 * 개념: 로터리 엔코더는 90° 위상 차이가 나는 A/B 두 신호를 낸다. 어느 쪽이 먼저 바뀌는지로 회전 방향을, 에지 수로 이동량을 안다.
 *       타이머가 두 입력을 직접 보고 CNT 를 증감(방향은 CR1.DIR)시키므로 CPU 부하가 0 이다. (인터럽트 없이도 고속 회전 추적)
 *
 * 설정 (RM0008 15.3.12)  TIM3, A = PA6(TI1), B = PA7(TI2)
 *   TIM3->SMCR.SMS = 011 : TI1 과 TI2 의 모든 에지에서 카운트 (x4 모드) -> 엔코더 1 주기(4 상태) = 4 카운트
 *   CCER.CC1P/CC2P : 극성,  CCMR1.IC1F/IC2F : 입력 필터(기계식 엔코더 접점 튐 제거)
 *   16비트 카운터는 오버플로하므로, 이동량은 "이전 값과의 16비트 부호 있는 차"로 계산하면 래핑에 안전하다.
 *
 * 시험 방법 두 가지
 *   (A) 실제 엔코더: A/B 를 PA6/PA7 에 연결(풀업 필요), 공통 GND.  SIMULATE_ENCODER 를 0 으로 바꿔 빌드
 *   (B) 시뮬레이션(기본): PB0 -> PA6, PB1 -> PA7 을 점퍼선으로 연결. 소프트웨어가 PB0/PB1 로 직교 신호를 만들어 낸다.
 *       B1(PC13)을 누를 때마다 회전 방향이 바뀐다.
 *
 * 출력: 200ms 마다 "count=… delta=… (+=정방향/-=역방향) rate=… counts/s"  (UART 115200)
 *
 * 인터럽트를 사용하지 않는다 (폴링). 이동 감지에 인터럽트가 필요하면 오버플로/CC 인터럽트를 추가.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define SIMULATE_ENCODER 1

TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Encoder_Init(void);

#if SIMULATE_ENCODER
/* 직교 신호 한 스텝 생성: 정방향 00->01->11->10, 역방향은 그 반대 (A=PB0, B=PB1) */
static void sim_step(int dir)
{
    static uint8_t state;                             /* 0..3 : Gray 코드 인덱스 */
    static const uint8_t gray[4] = { 0x0, 0x1, 0x3, 0x2 };   /* (B<<1)|A */
    state = (uint8_t)((state + (dir > 0 ? 1 : 3)) & 3U);
    uint8_t g = gray[state];
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (g & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* A */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, (g & 2U) ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* B */
}
#endif

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM3_Encoder_Init();

    char msg[96];
    uint16_t prev = 0;
    uint32_t t_print = HAL_GetTick(), t_step = HAL_GetTick();
    int dir = 1;
    (void)t_step; (void)dir;

    while (1)
    {
#if SIMULATE_ENCODER
        if (HAL_GetTick() - t_step >= 5)               /* 5ms 마다 1 스텝 = 200 스텝/s */
        {
            t_step += 5;
            sim_step(dir);
        }
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1: 방향 전환 */
        {
            HAL_Delay(30);
            dir = -dir;
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        }
#endif
        if (HAL_GetTick() - t_print >= 200)
        {
            t_print += 200;
            uint16_t now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);   /* TIM3->CNT */
            int16_t delta = (int16_t)(now - prev);                    /* 16비트 래핑에 안전한 부호 있는 차 */
            prev = now;
            int dir_hw = (htim3.Instance->CR1 & TIM_CR1_DIR) ? -1 : +1;   /* CR1.DIR : 0=증가, 1=감소 */
            int n = snprintf(msg, sizeof msg, "count=%5u  delta=%+5d  dir=%c  rate=%+ld counts/s\r\n",
                             now, delta, dir_hw > 0 ? '+' : '-', (long)delta * 5L);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;                  /* TI1=PA6, TI2=PA7 */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;                              /* 오픈 컬렉터 엔코더 대비 풀업 (CNF=10, ODR=1) */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_TIM3_Encoder_Init(void)
{
    TIM_Encoder_InitTypeDef e = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFF;                        /* 16비트 전체 범위 */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    e.EncoderMode = TIM_ENCODERMODE_TI12;              /* SMCR.SMS = 011 : x4 */
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1;
    e.IC1Filter = 6;                                   /* 디지털 필터: 접점 튐 제거 (0~15, 클수록 강함) */
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1;
    e.IC2Filter = 6;
    if (HAL_TIM_Encoder_Init(&htim3, &e) != HAL_OK) Error_Handler();
    if (HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;                   /* 시뮬레이션용 A/B 출력 */
    g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;    /* B1 */
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

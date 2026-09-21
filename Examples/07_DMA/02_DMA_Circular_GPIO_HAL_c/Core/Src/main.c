/**
 * 02_DMA_Circular_GPIO_HAL_c  —  [계층: HAL]  원형(circular) DMA + 타이머 요청으로 GPIO 포트에 패턴을 자동 출력 (핑퐁 버퍼)
 *
 * CPU 없이 8비트 패턴을 정확한 주기로 GPIOC 에 내보낸다. 로직 분석기, LED 8개(저항 포함), 병렬 DAC(R-2R) 등을 연결해 관찰.
 *
 * 구성
 *   TIM2 업데이트 이벤트(10kHz) --DMA 요청--> DMA1 채널2 --> GPIOC->ODR (반워드 전송)
 *   출력 핀: PC0..PC7 (Nucleo morpho). 버퍼 = 64 샘플, 원형 모드라 끝에 닿으면 처음부터 무한 반복.
 *   패턴 주기 = 64 샘플 / 10kHz = 6.4ms -> 약 156Hz 로 반복
 *
 * 핑퐁(double buffering)
 *   DMA 가 버퍼의 앞 절반을 내보내는 동안 CPU 가 뒤 절반을 채우고, 뒤 절반을 내보내는 동안 앞 절반을 채우는 방식.
 *   반 버퍼 전송 완료(HT) / 전체 완료(TC) 인터럽트가 "지금 다 내보낸 쪽이 비었으니 새 데이터를 채우라"는 신호다.
 *   이 예제: 콜백이 비워진 쪽 절반을 "다음 패턴"으로 채운다. 패턴은 1초마다 바뀌는 3종
 *     (0) 나이트라이더(단일 비트 이동) (1) 8비트 이진 카운터 (2) 삼각파(램프 업/다운, 병렬 DAC 에 연결하면 삼각 신호)
 *
 * 하드웨어 대응 (RM0008 13장)
 *   TIM2->DIER.UDE(bit8) = 1 : 업데이트 이벤트마다 DMA 요청
 *   DMA1_Channel2: 메모리->주변장치, 소스 메모리 주소 증가, 목적지(ODR) 고정, 반워드(16비트) 단위, CCR.CIRC=1, HTIE/TCIE
 *   *** GPIOC->ODR 전체(16비트)를 쓰므로 PC8..PC15 는 0 으로 구동된다. PC13(B1)은 입력 모드라 영향이 없지만,
 *       PC8..PC15 에 다른 출력을 쓰는 응용에서는 BSRR 로 필요한 비트만 쓰도록 바꿔야 한다. ***
 *
 * ISR vs 콜백
 *   DMA1_Channel2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> half_cb (HT) / full_cb (TC)  (HAL_DMA_RegisterCallback 으로 등록한 이 파일의 함수)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define N 64                                  /* 전체 샘플 수 (반 = 32) */

TIM_HandleTypeDef htim2;
DMA_HandleTypeDef hdma_tim2_up;
UART_HandleTypeDef huart2;

static uint16_t buf[N];
static volatile uint8_t pattern;              /* 0/1/2 : main 이 1초마다 변경 */
static volatile uint32_t half_count, full_count;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_DMA_Init(void);

/* 반 버퍼(32개)를 현재 패턴으로 채운다. phase = 이 절반이 패턴 안에서 시작하는 위치 */
static void fill_half(uint16_t *p, uint32_t start)
{
    for (uint32_t i = 0; i < N / 2; i++)
    {
        uint32_t k = start + i;               /* 패턴 안의 샘플 번호 */
        uint16_t v = 0;
        switch (pattern)
        {
        case 0:  v = (uint16_t)(1u << ((k / 4) & 7));  break;                           /* 나이트라이더: 4샘플마다 한 칸 */
        case 1:  v = (uint16_t)((k * 4) & 0xFF);       break;                           /* 이진 카운터(램프) */
        default: { uint32_t t = k & 0x3F; v = (uint16_t)((t < 32 ? t : 63 - t) * 8); }  /* 삼각파 0..248 */
        }
        p[i] = v;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    fill_half(&buf[0], 0);
    fill_half(&buf[N / 2], N / 2);
    MX_TIM2_DMA_Init();

    /* DMA 시작: 메모리 buf -> GPIOC->ODR, 원형. TIM2 업데이트마다 1샘플 */
    if (HAL_DMA_Start_IT(&hdma_tim2_up, (uint32_t)buf, (uint32_t)&GPIOC->ODR, N) != HAL_OK) Error_Handler();
    __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_UPDATE);        /* DIER.UDE = 1 */
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK) Error_Handler();

    char msg[96];
    uint32_t t_sw = HAL_GetTick();
    while (1)
    {
        if (HAL_GetTick() - t_sw >= 1000)
        {
            t_sw += 1000;
            pattern = (uint8_t)((pattern + 1) % 3);      /* 콜백들이 다음 반 버퍼부터 새 패턴으로 채운다 */
            static const char *const nm[3] = { "knight-rider", "binary ramp", "triangle" };
            int n = snprintf(msg, sizeof msg, "pattern=%s  HT=%lu TC=%lu (기대: 각각 약 156/s)\r\n",
                             nm[pattern], (unsigned long)half_count, (unsigned long)full_count);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* DMA 콜백 (ISR 문맥): HT = 앞 절반 전송 완료, TC = 뒤 절반 전송 완료                */
/* ------------------------------------------------------------------------- */
static void half_cb(DMA_HandleTypeDef *h) { (void)h; fill_half(&buf[0], 0);          half_count++; }   /* 앞 절반이 비었음 -> 채움 */
static void full_cb(DMA_HandleTypeDef *h) { (void)h; fill_half(&buf[N / 2], N / 2);  full_count++; }   /* 뒤 절반이 비었음 -> 채움 */

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
}

static void MX_TIM2_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* TIM2 : 64MHz / (PSC+1) / (ARR+1) = 10kHz  -> PSC = 63, ARR = 99 (64MHz/64 = 1MHz, /100 = 10kHz) */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 100 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    hdma_tim2_up.Instance = DMA1_Channel2;                          /* TIM2_UP 는 DMA1 채널2 (RM0008 13.3.7) */
    hdma_tim2_up.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim2_up.Init.PeriphInc = DMA_PINC_DISABLE;                 /* ODR 주소 고정 */
    hdma_tim2_up.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim2_up.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim2_up.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim2_up.Init.Mode = DMA_CIRCULAR;                          /* CCR.CIRC = 1 */
    hdma_tim2_up.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_tim2_up) != HAL_OK) Error_Handler();
    HAL_DMA_RegisterCallback(&hdma_tim2_up, HAL_DMA_XFER_HALFCPLT_CB_ID, half_cb);
    HAL_DMA_RegisterCallback(&hdma_tim2_up, HAL_DMA_XFER_CPLT_CB_ID, full_cb);

    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = 0x00FF;                                                 /* PC0..PC7 출력 */
    g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_HIGH;
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

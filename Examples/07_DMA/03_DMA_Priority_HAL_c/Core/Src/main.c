/**
 * 03_DMA_Priority_HAL_c  —  [계층: HAL]  DMA 채널 우선순위와 버스 중재: 두 채널이 동시에 전송할 때 누가 먼저 끝나는가
 *
 * 실험: DMA1 채널1 과 채널2 가 각각 4KB(1024워드) 메모리->메모리 복사를 "동시에" 시작한다.
 *       채널의 우선순위 설정에 따라 어느 채널이 먼저 끝나는지, 끝나는 시간이 어떻게 달라지는지 DWT 사이클로 측정한다.
 *
 * 중재 규칙 (RM0008 13.3.2 DMA 처리, "Arbiter")
 *   1) 소프트웨어 우선순위 (CCR.PL[1:0]): Very high > High > Medium > Low
 *   2) 소프트웨어 우선순위가 같으면 채널 번호가 낮은 쪽이 우선 (채널1 > 채널2 > ... > 채널7)
 *   중재는 "전송 1개(데이터 1워드) 단위"로 이루어진다. 높은 우선순위 채널이 요청을 계속 내는 동안에는 낮은 채널이 밀린다.
 *   메모리->메모리 모드는 요청이 항상 활성이라 우선순위 차이가 가장 극적으로 드러난다.
 *
 * B1(PC13)을 누를 때마다 설정이 바뀐다 (각 설정에서 시험을 3회 반복해 UART(115200)로 출력)
 *   케이스 A: 채널1 = Low,       채널2 = Very high   -> 채널2 가 먼저 끝나고, 채널1 은 그 뒤에 이어서 진행
 *   케이스 B: 채널1 = Very high, 채널2 = Low         -> 반대
 *   케이스 C: 둘 다 Medium                          -> 우선순위 같음: 낮은 번호(채널1)가 먼저
 *   출력: 각 채널의 완료 시각(시작 후 사이클 수)과 먼저 끝난 채널
 *
 * 인터럽트: 각 채널의 TC 인터럽트가 완료 시각(DWT->CYCCNT)만 기록한다.
 *   DMA1_Channel1/2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler() -> done1_cb / done2_cb (이 파일)
 *   * 인터럽트 진입 지연이 측정에 약간 섞이지만 채널 간 순서와 큰 차이를 보는 데는 충분하다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define WORDS 1024

DMA_HandleTypeDef hdma1;      /* 채널1 */
DMA_HandleTypeDef hdma2;      /* 채널2 */
UART_HandleTypeDef huart2;

static uint32_t src[WORDS];
static uint32_t dst1[WORDS], dst2[WORDS];
static volatile uint32_t t_start, t_done1, t_done2;
static volatile uint8_t done1, done2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void dma_setup(uint32_t pl1, uint32_t pl2);

static void done1_cb(DMA_HandleTypeDef *h) { (void)h; t_done1 = DWT->CYCCNT - t_start; done1 = 1; }
static void done2_cb(DMA_HandleTypeDef *h) { (void)h; t_done2 = DWT->CYCCNT - t_start; done2 = 1; }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    for (uint32_t i = 0; i < WORDS; i++) src[i] = 0xC0DE0000u + i;
    __HAL_RCC_DMA1_CLK_ENABLE();

    static const struct { uint32_t p1, p2; const char *name; } cases[3] = {
        { DMA_PRIORITY_LOW,       DMA_PRIORITY_VERY_HIGH, "A: ch1=Low,       ch2=Very high" },
        { DMA_PRIORITY_VERY_HIGH, DMA_PRIORITY_LOW,       "B: ch1=Very high, ch2=Low      " },
        { DMA_PRIORITY_MEDIUM,    DMA_PRIORITY_MEDIUM,    "C: ch1=Medium,    ch2=Medium   " },
    };

    char msg[128];
    int c = 0;
    dma_setup(cases[c].p1, cases[c].p2);
    while (1)
    {
        for (int rep = 0; rep < 3; rep++)
        {
            memset(dst1, 0, sizeof dst1); memset(dst2, 0, sizeof dst2);
            done1 = done2 = 0;

            /* 두 채널을 가능한 한 동시에 시작 (인터럽트 금지 상태에서 연달아 시작) */
            __disable_irq();
            HAL_DMA_Start_IT(&hdma1, (uint32_t)src, (uint32_t)dst1, WORDS);
            HAL_DMA_Start_IT(&hdma2, (uint32_t)src, (uint32_t)dst2, WORDS);
            t_start = DWT->CYCCNT;
            __enable_irq();

            uint32_t t0 = HAL_GetTick();
            while ((!done1 || !done2) && (HAL_GetTick() - t0) < 100) {}

            int ok = (memcmp(src, dst1, sizeof src) == 0) && (memcmp(src, dst2, sizeof src) == 0);
            int n = snprintf(msg, sizeof msg, "[%s] ch1 done @ %6lu cyc | ch2 done @ %6lu cyc | first: ch%d | copy %s\r\n",
                             cases[c].name, (unsigned long)t_done1, (unsigned long)t_done2,
                             (t_done1 <= t_done2) ? 1 : 2, ok ? "OK" : "FAIL");
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
            HAL_Delay(300);
        }
        HAL_UART_Transmit(&huart2, (uint8_t *)"--- press B1 for next case ---\r\n", 32, 100);
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {}   /* B1 대기 */
        HAL_Delay(30);
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        HAL_Delay(30);
        c = (c + 1) % 3;
        dma_setup(cases[c].p1, cases[c].p2);
    }
}

/* 두 채널을 메모리->메모리로 다시 설정 (우선순위만 바꿔서) */
static void dma_setup(uint32_t pl1, uint32_t pl2)
{
    DMA_HandleTypeDef *h[2] = { &hdma1, &hdma2 };
    DMA_Channel_TypeDef *ch[2] = { DMA1_Channel1, DMA1_Channel2 };
    uint32_t pl[2] = { pl1, pl2 };
    for (int i = 0; i < 2; i++)
    {
        h[i]->Instance = ch[i];
        HAL_DMA_DeInit(h[i]);
        h[i]->Init.Direction = DMA_MEMORY_TO_MEMORY;             /* CCR.MEM2MEM = 1 */
        h[i]->Init.PeriphInc = DMA_PINC_ENABLE;
        h[i]->Init.MemInc = DMA_MINC_ENABLE;
        h[i]->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        h[i]->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        h[i]->Init.Mode = DMA_NORMAL;
        h[i]->Init.Priority = pl[i];                             /* CCR.PL[1:0] */
        if (HAL_DMA_Init(h[i]) != HAL_OK) Error_Handler();
    }
    HAL_DMA_RegisterCallback(&hdma1, HAL_DMA_XFER_CPLT_CB_ID, done1_cb);
    HAL_DMA_RegisterCallback(&hdma2, HAL_DMA_XFER_CPLT_CB_ID, done2_cb);
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
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

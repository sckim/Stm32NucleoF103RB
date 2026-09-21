/**
 * 60_DMA_MemToMem_HAL_c  —  [계층: HAL]  DMA1 채널1 메모리->메모리 복사
 *
 * 동작
 *   - 256워드(1KB) 배열을 (1) CPU 반복문, (2) DMA 로 각각 복사하고 DWT 사이클 카운터로 시간을 비교한다.
 *   - 부팅 시 1회, 이후 B1(PC13) 버튼을 누를 때마다 다시 측정하여 USART2(115200)로 출력.
 *
 * DMA 설정 (RM0008 13장)
 *   DMA1_Channel1->CCR: MEM2MEM=1, DIR=1(주변장치 주소 = 소스), PINC=1, MINC=1, PSIZE=MSIZE=32비트
 *   DMA1_Channel1->CPAR = 소스 주소, CMAR = 목적지 주소, CNDTR = 전송 개수
 *   DMA 는 CPU 를 쓰지 않고 버스 마스터로 복사 -> 완료되면 ISR.TCIF1 -> 인터럽트
 *
 * ISR vs 콜백
 *   DMA1_Channel1_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> dma_done_cb() (이 파일, HAL_DMA_RegisterCallback 으로 등록한 콜백)
 *
 * 참고: 데이터가 SRAM 안에서 왕복하므로 버스 경합 때문에 DMA 가 CPU 루프보다 극적으로
 *       빠르지는 않을 수 있다. 이 예제의 핵심은 "전송 중 CPU 가 자유로움"을 확인하는 것이다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define N_WORDS 256

DMA_HandleTypeDef hdma_m2m;
UART_HandleTypeDef huart2;

static uint32_t src[N_WORDS];
static uint32_t dst_cpu[N_WORDS];
static uint32_t dst_dma[N_WORDS];
static volatile uint8_t dma_done;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_DMA_Init(void);
static void run_test(void);

static void dma_done_cb(DMA_HandleTypeDef *hdma) { (void)hdma; dma_done = 1; }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_DMA_Init();

    /* DWT 사이클 카운터: CoreDebug->DEMCR.TRCENA=1, DWT->CTRL.CYCCNTENA=1 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (uint32_t i = 0; i < N_WORDS; i++) src[i] = 0xA5000000u | i;

    run_test();
    while (1)
    {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* IDR13 == 0 : 눌림 */
        {
            HAL_Delay(30);                                            /* 간이 디바운스 */
            run_test();
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(30);
        }
    }
}

static void run_test(void)
{
    char msg[96];
    uint32_t t0, cpu_cycles, dma_cycles, free_loops = 0;

    memset(dst_cpu, 0, sizeof dst_cpu);
    memset(dst_dma, 0, sizeof dst_dma);

    /* (1) CPU 복사. volatile 로 컴파일러가 memcpy 로 바꾸거나 제거하지 못하게 한다 */
    volatile uint32_t *d = dst_cpu;
    t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < N_WORDS; i++) d[i] = src[i];
    cpu_cycles = DWT->CYCCNT - t0;

    /* (2) DMA 복사: 시작 후 완료될 때까지 CPU 가 다른 일을 한 횟수(free_loops)도 센다 */
    dma_done = 0;
    t0 = DWT->CYCCNT;
    if (HAL_DMA_Start_IT(&hdma_m2m, (uint32_t)src, (uint32_t)dst_dma, N_WORDS) != HAL_OK) Error_Handler();
    while (!dma_done) free_loops++;
    dma_cycles = DWT->CYCCNT - t0;

    int ok = (memcmp(src, dst_dma, sizeof src) == 0);
    int n = snprintf(msg, sizeof msg, "CPU=%lu cyc  DMA=%lu cyc  (CPU free loops=%lu)  verify=%s\r\n",
                     (unsigned long)cpu_cycles, (unsigned long)dma_cycles,
                     (unsigned long)free_loops, ok ? "OK" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();                      /* RCC->AHBENR.DMA1EN */

    hdma_m2m.Instance = DMA1_Channel1;
    hdma_m2m.Init.Direction = DMA_MEMORY_TO_MEMORY;   /* CCR.MEM2MEM=1 */
    hdma_m2m.Init.PeriphInc = DMA_PINC_ENABLE;        /* CCR.PINC : 소스 주소 증가 */
    hdma_m2m.Init.MemInc = DMA_MINC_ENABLE;           /* CCR.MINC : 목적지 주소 증가 */
    hdma_m2m.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;   /* CCR.PSIZE = 32비트 */
    hdma_m2m.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;      /* CCR.MSIZE = 32비트 */
    hdma_m2m.Init.Mode = DMA_NORMAL;                  /* CCR.CIRC = 0 */
    hdma_m2m.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_m2m) != HAL_OK) Error_Handler();

    HAL_DMA_RegisterCallback(&hdma_m2m, HAL_DMA_XFER_CPLT_CB_ID, dma_done_cb);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);           /* NVIC->ISER[0] bit11 */
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX */
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

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_13;                /* B1: 외부 풀업, 눌리면 Low */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);
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

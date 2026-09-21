/**
 * 02_SPI_DMA_Fullduplex_HAL_c  —  [계층: HAL]  SPI1 전이중 DMA 전송 (루프백 시험)
 *
 * 배선: PA7 (MOSI) ---- 점퍼선 ---- PA6 (MISO)   (01_SPI_Loopback 과 동일)
 *
 * 동작
 *   - 512바이트를 SPI1(8MHz)로 송수신하는 동안 DMA 가 전송을 맡고, CPU 는 다른 일을 한 횟수(free_loops)를 센다.
 *   - 폴링 방식(HAL_SPI_TransmitReceive)과 소요 시간, CPU 여유를 비교하여 UART(115200)로 출력.
 *   - OLED/TFT/SD 카드처럼 큰 데이터를 SPI 로 보내는 장치에서 DMA 가 필수적인 이유를 보여준다.
 *
 * DMA 채널 매핑 (RM0008 13.3.7 DMA request mapping)
 *   SPI1_RX -> DMA1 채널2,  SPI1_TX -> DMA1 채널3
 *   SPI1->CR2.RXDMAEN / TXDMAEN 으로 DMA 요청 허용
 *   TX: 메모리->주변장치(SPI1->DR), RX: 주변장치->메모리, 메모리 주소만 증가, 바이트 단위
 *
 * ISR vs 콜백
 *   DMA1_Channel2_IRQHandler / DMA1_Channel3_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_DMA_IRQHandler()
 *     -> HAL_SPI_TxRxCpltCallback() (이 파일) : 송수신 모두 끝났을 때 1회 호출
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define LEN 512

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;
UART_HandleTypeDef huart2;

static uint8_t tx[LEN], rx[LEN];
static volatile uint8_t spi_done;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_SPI1_Init();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (int i = 0; i < LEN; i++) tx[i] = (uint8_t)(i ^ 0x5A);

    char msg[128];
    while (1)
    {
        /* (1) 폴링: CPU 가 전송이 끝날 때까지 묶여 있음 */
        memset(rx, 0, sizeof rx);
        uint32_t t0 = DWT->CYCCNT;
        HAL_StatusTypeDef s1 = HAL_SPI_TransmitReceive(&hspi1, tx, rx, LEN, 200);
        uint32_t poll_cyc = DWT->CYCCNT - t0;
        int ok1 = (s1 == HAL_OK) && (memcmp(tx, rx, LEN) == 0);

        /* (2) DMA: 시작만 하고 CPU 는 다른 일 (여기서는 카운터 증가로 흉내) */
        memset(rx, 0, sizeof rx);
        spi_done = 0;
        uint32_t free_loops = 0;
        t0 = DWT->CYCCNT;
        if (HAL_SPI_TransmitReceive_DMA(&hspi1, tx, rx, LEN) != HAL_OK) Error_Handler();
        while (!spi_done) free_loops++;
        uint32_t dma_cyc = DWT->CYCCNT - t0;
        int ok2 = (memcmp(tx, rx, LEN) == 0);

        int n = snprintf(msg, sizeof msg, "poll=%lu us [%s]  DMA=%lu us [%s]  CPU free loops during DMA=%lu\r\n",
                         (unsigned long)(poll_cyc / 64), ok1 ? "PASS" : "FAIL",
                         (unsigned long)(dma_cyc / 64), ok2 ? "PASS" : "FAIL",
                         (unsigned long)free_loops);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);
        HAL_Delay(1000);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): DMA 전이중 전송 완료                                     */
/* ------------------------------------------------------------------------- */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) spi_done = 1;
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* SCK, MOSI */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_6;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;                          /* MISO */
    HAL_GPIO_Init(GPIOA, &g);

    hdma_spi1_rx.Instance = DMA1_Channel2;                       /* SPI1_RX */
    hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_rx.Init.Mode = DMA_NORMAL;
    hdma_spi1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_spi1_rx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hspi, hdmarx, hdma_spi1_rx);

    hdma_spi1_tx.Instance = DMA1_Channel3;                       /* SPI1_TX */
    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_tx.Init.Mode = DMA_NORMAL;
    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hspi, hdmatx, hdma_spi1_tx);

    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 64MHz / 8 = 8MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
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

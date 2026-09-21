/**
 * 12_USART_DMA_IdleLine_HAL_c  —  [계층: HAL]  USART2 DMA 수신 + IDLE 라인 감지 (가변 길이 패킷)
 *
 * 동작
 *   - PC 터미널(115200 8N1)에서 임의 길이의 문자열을 한 번에 보내면(한 줄 전송 등),
 *     DMA 가 CPU 개입 없이 수신 버퍼에 채우고, "라인이 조용해지는 순간(IDLE)" 한 번의 이벤트로
 *     패킷 길이와 내용을 알려 준다.  -> "[RX n bytes] ..." 로 되돌려 출력.
 *   - 바이트마다 인터럽트가 걸리는 11_USART_Interrupt_RX 와 비교: 인터럽트 횟수가 패킷당 1회로 줄어든다.
 *
 * 하드웨어 대응 (RM0008)
 *   USART2_RX  -> DMA1 채널6 (DMA1_Channel6->CPAR = &USART2->DR, CMAR = 수신 버퍼)
 *   USART2->CR3.DMAR = 1   : 수신 DMA 요청 허용
 *   USART2->CR1.IDLEIE = 1 : IDLE 인터럽트 허용 (SR.IDLE : 마지막 수신 후 1프레임 시간 동안 무신호)
 *   DMA CCR.CIRC = 1       : 원형 모드 (버퍼 끝에 닿으면 처음으로 되돌아감)
 *
 * ISR vs 콜백
 *   USART2_IRQHandler / DMA1_Channel6_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler / HAL_DMA_IRQHandler
 *     -> HAL_UARTEx_RxEventCallback() (이 파일) : IDLE 또는 DMA 전송 완료(TC) 시 호출, Size = 버퍼 내 현재 위치
 *   콜백은 ISR 문맥이므로 "패킷 복사 + 플래그"만 하고 출력은 main 에서 한다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define DMA_BUF_SIZE 128     /* DMA 원형 수신 버퍼 */
#define PKT_MAX      128

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

static uint8_t dma_buf[DMA_BUF_SIZE];
static uint16_t old_pos;                 /* 마지막으로 처리한 dma_buf 위치 */

static uint8_t pkt[PKT_MAX];             /* 콜백 -> main 전달용 패킷 */
static volatile uint16_t pkt_len;
static volatile uint8_t pkt_ready;
static volatile uint32_t pkt_dropped;    /* main 이 처리 못 해서 버려진 패킷 수 */
static volatile uint32_t event_count;    /* RxEvent 콜백(=인터럽트 이벤트) 횟수 */

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    const char *banner = "\r\n[USART2 DMA + IDLE] 문자열을 보내 보세요\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)banner, (uint16_t)strlen(banner), 100);

    /* DMA 수신 + IDLE 이벤트 시작. 반 버퍼(HT) 이벤트는 끄고 IDLE/TC 만 받는다 */
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, dma_buf, DMA_BUF_SIZE) != HAL_OK) Error_Handler();
    __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);          /* DMA1_Channel6->CCR.HTIE = 0 */

    char head[40];
    while (1)
    {
        if (pkt_ready)
        {
            uint16_t len = pkt_len;
            int n = snprintf(head, sizeof head, "[RX %u bytes, events=%lu, drop=%lu] ",
                             (unsigned)len, (unsigned long)event_count, (unsigned long)pkt_dropped);
            HAL_UART_Transmit(&huart2, (uint8_t *)head, (uint16_t)n, 100);
            HAL_UART_Transmit(&huart2, pkt, len, 200);
            HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);
            pkt_ready = 0;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): IDLE 감지 또는 DMA 버퍼 끝 도달 시 HAL 이 호출                 */
/* ------------------------------------------------------------------------- */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART2) return;
    event_count++;

    /* Size = dma_buf 안에서 "다음에 쓸 위치". 원형 버퍼이므로 old_pos -> Size 구간이 새 데이터 */
    uint16_t len = 0;
    if (Size != old_pos)
    {
        if (pkt_ready)
        {
            pkt_dropped++;                                   /* main 이 아직 이전 패킷을 안 가져감 */
        }
        else
        {
            if (Size > old_pos)
            {
                len = Size - old_pos;
                memcpy(pkt, &dma_buf[old_pos], len);
            }
            else                                             /* 버퍼 끝을 넘어 래핑된 경우 */
            {
                uint16_t first = DMA_BUF_SIZE - old_pos;
                memcpy(pkt, &dma_buf[old_pos], first);
                memcpy(pkt + first, dma_buf, Size);
                len = first + Size;
            }
            pkt_len = len;
            pkt_ready = 1;
        }
    }
    old_pos = (Size >= DMA_BUF_SIZE) ? 0 : Size;
}

/* HAL_UART_Init 이 호출: 클럭/핀/DMA/NVIC */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_USART2_CLK_ENABLE();   /* RCC->APB1ENR.USART2EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN   */
    __HAL_RCC_DMA1_CLK_ENABLE();     /* RCC->AHBENR.DMA1EN    */

    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX */
    HAL_GPIO_Init(GPIOA, &g);

    hdma_usart2_rx.Instance = DMA1_Channel6;                    /* USART2_RX 는 DMA1 채널6 고정 */
    hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;           /* 주변장치(DR) 주소는 고정 */
    hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;               /* 메모리 주소는 증가 */
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode = DMA_CIRCULAR;                    /* CCR.CIRC = 1 */
    hdma_usart2_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);               /* huart->hdmarx 연결 */

    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);                    /* IDLE 인터럽트용 */
    HAL_NVIC_EnableIRQ(USART2_IRQn);
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

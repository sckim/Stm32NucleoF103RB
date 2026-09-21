/**
 * 06_USART_HalfDuplex_HAL_c  —  [계층: HAL]  USART 단선(Half-duplex) 모드: 한 가닥 선으로 송수신 (RS-485/스마트 서보 방식)
 *
 * 단선 모드란 (RM0008 27장 USART, "Single-wire half-duplex communication")
 *   TX 핀 하나가 송신과 수신을 번갈아 쓴다. 내부적으로 RX 를 TX 핀에 연결하고 CR3.HDSEL=1 로 켠다.
 *   버스에는 오픈드레인 + 풀업이 필요하고, "지금 누가 말하는가"를 프로토콜로 정해야 한다(마스터가 묻고 슬레이브가 답하는 방식이 흔함).
 *   HAL_HalfDuplex_EnableTransmitter()/EnableReceiver() 로 방향을 바꾼다 (CR1.TE / CR1.RE 전환).
 *
 * 이 예제는 보드 한 장으로 시험하도록 구성했다
 *   [마스터] USART1 (단선 모드, PA9)                         [가짜 슬레이브] USART3 (일반 전이중, PB10=TX, PB11=RX)
 *   버스 선 하나:  PA9 ----+---- PB11 (USART3_RX)
 *                          +---- PB10 (USART3_TX)     -> 세 핀을 점퍼선으로 모두 연결하고 3.3V 풀업(4.7kΩ) 권장
 *   동작: 마스터가 "PING" 송신 -> 수신 모드로 전환 -> 슬레이브가 "PONG" 으로 응답 -> 마스터가 수신 확인.
 *         (슬레이브 USART3 는 자기가 보낸 데이터를 RX 로 다시 듣게 되므로 그 4바이트는 버린다)
 *   결과는 ST-Link 가상 COM(USART2, 115200)으로 1초마다 출력.
 *
 * 하드웨어 대응
 *   USART1->CR3.HDSEL(bit3) = 1 : 단선 반이중,  PA9 는 대체기능 오픈드레인(CNF=11) + 내부/외부 풀업
 *   송신 후에는 SR.TC(전송 완료)를 확인한 다음 수신으로 바꿔야 마지막 바이트가 잘리지 않는다 (HAL_UART_Transmit 이 TC 까지 기다림)
 *
 * 인터럽트를 사용하지 않는다 (폴링). 실제 RS-485 에서는 트랜시버의 DE/RE 핀을 방향 전환 시점에 GPIO 로 제어한다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;    /* 로그 출력 (가상 COM) */
UART_HandleTypeDef huart1;    /* 마스터: 단선 모드 */
UART_HandleTypeDef huart3;    /* 가짜 슬레이브 */

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_HalfDuplex_Init(void);
static void MX_USART3_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_USART1_HalfDuplex_Init();
    MX_USART3_UART_Init();

    char msg[96];
    uint32_t ok = 0, fail = 0, seq = 0;
    while (1)
    {
        uint8_t req[4] = { 'P', 'I', 'N', 'G' };
        uint8_t rsp[4] = {0};
        uint8_t slave_rx[8] = {0};
        HAL_StatusTypeDef st_slave = HAL_ERROR, st_master = HAL_ERROR;

        /* 1) 마스터: 송신 모드로 바꾸고 "PING" 전송 (마지막 바이트 TC 까지 대기) */
        HAL_HalfDuplex_EnableTransmitter(&huart1);
        HAL_UART_Transmit(&huart1, req, 4, 50);

        /* 2) 슬레이브: 버스에서 "PING" 수신 */
        st_slave = HAL_UART_Receive(&huart3, slave_rx, 4, 50);

        if (st_slave == HAL_OK && memcmp(slave_rx, "PING", 4) == 0)
        {
            /* 3) 마스터: 수신 모드로 전환한 뒤 슬레이브가 "PONG" 응답 (자기 송신 4바이트는 자기 RX 로 되돌아오므로 읽어서 버림) */
            HAL_HalfDuplex_EnableReceiver(&huart1);
            uint8_t pong[4] = { 'P', 'O', 'N', 'G' };
            HAL_UART_Transmit(&huart3, pong, 4, 50);
            uint8_t echo[4];
            HAL_UART_Receive(&huart3, echo, 4, 50);              /* 자기 응답의 에코 제거 */
            st_master = HAL_UART_Receive(&huart1, rsp, 4, 50);   /* 마스터가 응답 수신 */
        }

        if (st_master == HAL_OK && memcmp(rsp, "PONG", 4) == 0) ok++; else fail++;

        int n = snprintf(msg, sizeof msg, "#%lu  slave got \"%.4s\"  master got \"%.4s\"  ok=%lu fail=%lu\r\n",
                         (unsigned long)seq++, slave_rx, rsp, (unsigned long)ok, (unsigned long)fail);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(1000);
    }
}

/* ---- MSP: 클럭/핀 (USART1, USART3 는 여기서 함께 처리) ---- */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef g = {0};
    if (huart->Instance == USART2)                      /* 가상 COM */
    {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else if (huart->Instance == USART1)                 /* 단선: PA9 하나만 */
    {
        __HAL_RCC_USART1_CLK_ENABLE();                  /* RCC->APB2ENR.USART1EN */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_9;
        g.Mode = GPIO_MODE_AF_OD;                       /* 오픈드레인 (CNF=11): 여러 장치가 같은 선을 공유 */
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else if (huart->Instance == USART3)                 /* 가짜 슬레이브: TX 는 오픈드레인, RX 는 풀업 */
    {
        __HAL_RCC_USART3_CLK_ENABLE();                  /* RCC->APB1ENR.USART3EN */
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin = GPIO_PIN_10;  g.Mode = GPIO_MODE_AF_OD;  g.Pull = GPIO_PULLUP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &g);
        g.Pin = GPIO_PIN_11;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOB, &g);
    }
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

static void MX_USART1_HalfDuplex_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 57600;                       /* 단선 버스는 보통 낮은 속도로 시작 */
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_HalfDuplex_Init(&huart1) != HAL_OK) Error_Handler();      /* CR3.HDSEL = 1, CR2.LINEN/CLKEN/SCEN/IREN = 0 */
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 57600;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
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

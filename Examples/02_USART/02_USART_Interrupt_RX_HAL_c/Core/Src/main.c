/**
 * 02_USART_Interrupt_RX_HAL_c  —  [계층: HAL]  USART2 수신 인터럽트
 *
 * 동작
 *   - PC 터미널(115200 8N1)에서 문자를 보내면 인터럽트로 1바이트씩 수신하여 에코한다.
 *   - '1' : LD2(PA5) ON, '0' : LD2 OFF, 't' : LD2 토글
 *
 * 핀 (Nucleo-F103RB, ST-Link 가상 COM 포트)
 *   USART2_TX = PA2, USART2_RX = PA3, LD2 = PA5
 *
 * 인터럽트 구조 (ISR vs 콜백)
 *   USART2_IRQHandler (stm32f1xx_it.c, 진짜 ISR)
 *     -> HAL_UART_IRQHandler()  : USART2->SR 의 RXNE / ORE 비트, CR1 의 RXNEIE 비트를 해석
 *       -> HAL_UART_RxCpltCallback() (이 파일, 사용자 콜백)  : 1바이트 수신 완료
 *   콜백은 ISR 문맥에서 실행되므로 "플래그만 세우고" 실제 처리는 main 루프에서 한다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

static uint8_t rx_byte;               /* HAL_UART_Receive_IT 가 채워 주는 1바이트 버퍼 */
static volatile uint8_t rx_data;      /* 콜백 -> main 으로 전달하는 데이터 */
static volatile uint8_t rx_ready;     /* 콜백 -> main 으로 전달하는 플래그 */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    const char *banner = "\r\n[USART2 RX interrupt] 1/0/t 를 입력하세요\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)banner, (uint16_t)strlen(banner), 100);

    /* 수신 인터럽트 시작: CR1.RXNEIE=1, CR3.EIE=1 (HAL 내부에서 설정) */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    while (1)
    {
        if (rx_ready)
        {
            uint8_t c = rx_data;
            rx_ready = 0;

            switch (c)
            {
            case '1': HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   break;  /* BSRR 로 set */
            case '0': HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); break;  /* BSRR 로 reset */
            case 't': HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);                break;  /* ODR XOR */
            default: break;
            }
            HAL_UART_Transmit(&huart2, &c, 1, 100);   /* 에코 (main 문맥에서 블로킹 송신) */
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 이 아님: HAL_UART_IRQHandler 가 ISR 안에서 호출해 주는 함수)    */
/* ------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rx_data = rx_byte;
        rx_ready = 1;
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);   /* 다음 바이트를 위해 재무장(필수) */
    }
}

/* 오버런/프레이밍 오류가 나면 HAL 이 수신을 중단하므로 여기서 다시 시작한다 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

/* HAL_UART_Init 이 내부에서 호출: 클럭/핀/NVIC 설정 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;

    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();   /* RCC->APB1ENR.USART2EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN   */

    g.Pin = GPIO_PIN_2;              /* TX: 대체기능 푸시풀 (CRL.CNF2=10, MODE2=11) */
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_3;              /* RX: 플로팅 입력 (CRL.CNF3=01, MODE3=00) */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);   /* NVIC->IP[38] */
    HAL_NVIC_EnableIRQ(USART2_IRQn);           /* NVIC->ISER[1] bit6 */
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;             /* BRR = APB1(32MHz)/115200 */
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
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;                        /* LD2 */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
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

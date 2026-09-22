/**
 * 09_MCO_ClockOut_HAL_c  —  [계층: HAL]  MCO(Microcontroller Clock Output): 내부 클럭을 핀으로 내보내 오실로스코프로 확인
 *
 * 동작
 *   - PA8 ('D7') 에 MCO 출력. B1(PC13)을 누를 때마다 출력 소스가 순환한다:
 *       1) HSI      = 8MHz   (내부 RC 오실레이터)
 *       2) PLL/2    = 32MHz  (PLL 출력 64MHz 의 절반. F1 의 MCO 는 PLL 을 항상 2 분주해서 내보낸다)
 *       3) HSE      = 8MHz   (Nucleo 는 ST-Link 의 MCO 를 HSE 로 바이패스 입력. 해당 납땜 브리지가 없으면 자동으로 건너뜀)
 *   - 현재 소스와 SYSCLK 를 UART(115200)로 출력. 시스템 클럭이 실제로 몇 MHz 인지 스코프로 교차 검증하는 용도.
 *   * SYSCLK(64MHz)를 직접 내보내는 소스도 있지만 MCO 핀의 최대 주파수가 50MHz 라 이 예제에서는 사용하지 않는다.
 *     (04_Clock_Config 는 48MHz 로 SYSCLK 을 내보낸다)
 *
 * 하드웨어 대응 (RM0008 7.3.2 RCC_CFGR)
 *   RCC->CFGR.MCO[26:24] : 000=없음, 100=SYSCLK, 101=HSI, 110=HSE, 111=PLL/2
 *   PA8 을 대체기능 푸시풀 50MHz 로 설정해야 한다 (HAL_RCC_MCOConfig 가 함께 처리: GPIOA->CRH[3:0] = 0xB)
 *   HSE 바이패스: RCC->CR.HSEBYP=1, HSEON=1 -> HSERDY 대기 (외부에서 클럭 신호를 그대로 입력)
 *
 * 응용: 다른 칩/코덱/FPGA 에 기준 클럭을 공급하거나, 크리스털/PLL 설정이 제대로 됐는지 검증.
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

typedef struct { uint32_t src; const char *name; } mco_src_t;

static mco_src_t list[3];
static int n_src;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    list[n_src++] = (mco_src_t){ RCC_MCO1SOURCE_HSI, "HSI (8 MHz)" };
    list[n_src++] = (mco_src_t){ RCC_MCO1SOURCE_PLLCLK, "PLL/2 (32 MHz)" };

    /* HSE 바이패스(ST-Link MCO 입력) 시도. 실패해도 나머지 소스는 계속 사용 가능 */
    RCC_OscInitTypeDef o = {0};
    o.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    o.HSEState = RCC_HSE_BYPASS;
    o.PLL.PLLState = RCC_PLL_NONE;                    /* PLL 설정은 건드리지 않는다 */
    if (HAL_RCC_OscConfig(&o) == HAL_OK) list[n_src++] = (mco_src_t){ RCC_MCO1SOURCE_HSE, "HSE bypass (8 MHz)" };

    int idx = 0;
    char msg[96];
    for (;;)
    {
        HAL_RCC_MCOConfig(RCC_MCO, list[idx].src, RCC_MCODIV_1);   /* PA8 설정 + CFGR.MCO 선택 */
        int n = snprintf(msg, sizeof msg, "MCO(PA8) = %s   [SYSCLK=%lu Hz, HCLK=%lu Hz]\r\n", list[idx].name,
                         (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_RCC_GetHCLKFreq());
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {}   /* B1 눌림 대기 */
        HAL_Delay(30);
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        HAL_Delay(30);
        idx = (idx + 1) % n_src;
    }
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

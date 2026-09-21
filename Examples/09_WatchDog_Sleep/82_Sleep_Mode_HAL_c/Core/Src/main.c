/**
 * 82_Sleep_Mode_HAL_c  —  [계층: HAL + CMSIS]  Sleep 모드(WFI): 할 일이 없으면 CPU 클럭 정지
 *
 * 동작
 *   - B1(PC13) 을 누를 때마다 "바쁜 대기" <-> "WFI 슬립" 모드가 바뀐다 (LD2 는 두 모드 모두 0.5s 점멸).
 *   - 1초마다 "그 1초 동안 main 루프가 몇 바퀴 돌았는가"를 UART(115200)로 출력한다.
 *       바쁜 대기 모드 : 수십만~수백만 바퀴 (CPU 가 계속 일함)
 *       슬립 모드      : 약 1000 바퀴 (SysTick 1ms 인터럽트로 깰 때마다 1바퀴)  -> 나머지 시간 CPU 클럭 정지
 *   - 실제 전류 차이는 Nucleo 의 IDD 점퍼(JP6)에 전류계를 연결해 확인한다 (64MHz 동작 vs Sleep).
 *
 * Sleep 모드 (PM0056 4.4, RM0008 5장)
 *   WFI 명령 : "Wait For Interrupt" -> 코어 클럭(HCLK) 정지, 주변장치와 SRAM 은 계속 동작
 *   SCB->SCR.SLEEPDEEP = 0  : Sleep (1 이면 Stop/Standby)
 *   SCB->SCR.SLEEPONEXIT    : 1 이면 ISR 이 끝나자마자 다시 잠듦 (여기서는 0)
 *   깨우는 원인 : 우선순위가 허용하는 "모든" 인터럽트 (여기서는 SysTick 1ms, EXTI 버튼)
 *   웨이크업 지연이 가장 짧다(수 사이클) -> 인터럽트 응답이 중요한 저전력 대기에 적합.
 *
 * 디버깅 주의: Sleep 중에도 디버거가 붙어 있으려면 DBGMCU_CR.DBG_SLEEP=1 이 필요하다 (아래에서 설정).
 *
 * ISR vs 콜백: SysTick_Handler (stm32f1xx_it.c) 가 HAL_IncTick() 만 호출, 버튼은 폴링으로 읽는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    HAL_DBGMCU_EnableDBGSleepMode();                  /* DBGMCU->CR.DBG_SLEEP = 1: 슬립 중에도 디버그 연결 유지 */

    int sleep_mode = 0;
    uint32_t loops = 0;
    uint32_t t_led = HAL_GetTick(), t_print = HAL_GetTick();
    char msg[80];

    while (1)
    {
        loops++;

        uint32_t now = HAL_GetTick();
        if (now - t_led >= 500)  { t_led += 500;  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); }

        if (now - t_print >= 1000)
        {
            t_print += 1000;
            int n = snprintf(msg, sizeof msg, "[%s] loops/s = %lu\r\n",
                             sleep_mode ? "SLEEP(WFI)" : "BUSY-WAIT", (unsigned long)loops);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
            loops = 0;
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1 : 모드 전환 */
        {
            HAL_Delay(30);
            sleep_mode = !sleep_mode;
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(30);
            loops = 0;
        }

        if (sleep_mode)
        {
            __WFI();                                  /* 다음 인터럽트(SysTick 1ms 등)까지 코어 정지 */
        }
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;  /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_INPUT;      g.Pull = GPIO_NOPULL;                                /* B1 */
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

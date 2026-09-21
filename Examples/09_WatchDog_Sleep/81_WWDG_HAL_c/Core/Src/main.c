/**
 * 81_WWDG_HAL_c  —  [계층: HAL]  윈도우 워치독(WWDG) — "너무 늦게"뿐 아니라 "너무 빨리" 리프레시해도 리셋
 *
 * IWDG 와의 차이 (80번은 이름과 달리 WWDG 기본 사용 예제)
 *   IWDG : 독립 저속 클럭(LSI 40kHz), 타임아웃 전에만 리프레시하면 됨(늦는 것만 감시).
 *   WWDG : APB1 클럭 기반 7비트 다운카운터(T[6:0]). 카운터가 "윈도우 값 이하"일 때만 리프레시 허용.
 *          윈도우 밖(너무 이름) 리프레시, 0x3F 이하로 내려감(너무 늦음) 모두 시스템 리셋.
 *          -> 프로그램이 "정해진 타이밍에" 실행되는지까지 감시한다.
 *
 * 타이밍 계산 (PCLK1 = 32MHz)
 *   t_tick = 4096 x prescaler(8) / 32MHz = 1.024 ms
 *   카운터 시작 0x7F, 윈도우 0x50, 리셋 임계 0x3F
 *     리프레시 허용 구간 : 카운터 <= 0x50  ->  (0x7F-0x50) x 1.024 = 약 48 ms 이후 ~
 *     리셋(타임아웃)     : 카운터 < 0x40   ->  (0x7F-0x3F) x 1.024 = 약 65.5 ms
 *   => 리프레시는 약 48ms ~ 65ms 사이에만 해야 한다. 이 예제는 55ms 마다 리프레시.
 *
 * 하드웨어 대응
 *   WWDG->CR : WDGA(bit7)=1 활성화(한 번 켜면 끌 수 없음), T[6:0] 카운터
 *   WWDG->CFR: W[6:0] 윈도우, WDGTB[8:7] 프리스케일러, EWI(bit9) 조기 경고 인터럽트
 *   WWDG->SR : EWIF(bit0)
 *   RCC->CSR : WWDGRSTF(bit30) 리셋 원인 플래그, RMVF(bit24) 플래그 지우기
 *
 * 시험 방법
 *   - 기본: 55ms 주기 리프레시로 LD2 가 정상 점멸.
 *   - B1(PC13) 누름: 프로그램 "멈춤" 흉내(리프레시 중단) -> 약 65ms 뒤 리셋 -> 부팅 메시지에 WWDG 리셋 표시
 *   - EARLY_REFRESH_TEST 를 1 로 바꾸면 20ms 주기(너무 이름)로 리프레시 -> 계속 리셋되는 것을 확인
 *
 * ISR vs 콜백
 *   WWDG_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_WWDG_IRQHandler()
 *     -> HAL_WWDG_EarlyWakeupCallback() (이 파일) : 카운터가 0x40 이 되는 순간, 리셋 직전 "마지막 기회"
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define EARLY_REFRESH_TEST 0     /* 1: 윈도우 밖(너무 이른) 리프레시 시험 */

WWDG_HandleTypeDef hwwdg;
UART_HandleTypeDef huart2;

static volatile uint32_t ewi_count;   /* 조기 경고가 발생한 횟수 (리셋 직전이므로 보통 1 이후 리셋) */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_WWDG_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 리셋 원인 확인 (RCC->CSR) */
    char msg[80];
    const char *cause = __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? "WWDG (window watchdog)"
                      : __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)  ? "POWER-ON"
                      : "other (pin/software/IWDG)";
    int n = snprintf(msg, sizeof msg, "\r\n[boot] reset cause: %s\r\n", cause);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    __HAL_RCC_CLEAR_RESET_FLAGS();                       /* CSR.RMVF = 1 */

    MX_WWDG_Init();                                      /* 이 순간부터 워치독 동작, 정지 불가 */
    __HAL_DBGMCU_FREEZE_WWDG();                          /* 디버거 중단점에서 카운터 정지 (DBGMCU_CR) */

    while (1)
    {
#if EARLY_REFRESH_TEST
        HAL_Delay(20);                                   /* 윈도우(약 48ms) 이전 -> 리프레시 즉시 리셋 */
#else
        HAL_Delay(55);                                   /* 48ms < 55ms < 65ms : 허용 구간 */
#endif
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
        {
            while (1) {}                                 /* 프로그램 멈춤 흉내: 리프레시 없이 대기 -> WWDG 리셋 */
        }

        if (HAL_WWDG_Refresh(&hwwdg) != HAL_OK) Error_Handler();   /* WWDG->CR.T = 0x7F */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);           /* 55ms 마다 토글: 육안으로는 빠른 점멸 */
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님, HAL_WWDG_IRQHandler 가 호출)                            */
/* ------------------------------------------------------------------------- */
void HAL_WWDG_EarlyWakeupCallback(WWDG_HandleTypeDef *hwwdg_)
{
    (void)hwwdg_;
    ewi_count++;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);  /* 리셋 직전 LED 를 켜 두어 "마지막 기회"가 실행됐음을 표시 */
    /* 여기서 리프레시하면 리셋을 막을 수 있으나, 그러면 워치독의 의미가 없어진다.
       실제 제품에서는 중요한 상태를 백업 레지스터/플래시에 저장하는 용도로 쓴다 (약 1ms 남음). */
}

/* HAL_WWDG_Init 이 호출: 클럭/NVIC */
void HAL_WWDG_MspInit(WWDG_HandleTypeDef *hw)
{
    (void)hw;
    __HAL_RCC_WWDG_CLK_ENABLE();                         /* RCC->APB1ENR.WWDGEN */
    HAL_NVIC_SetPriority(WWDG_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(WWDG_IRQn);
}

static void MX_WWDG_Init(void)
{
    hwwdg.Instance = WWDG;
    hwwdg.Init.Prescaler = WWDG_PRESCALER_8;             /* CFR.WDGTB = 11 */
    hwwdg.Init.Window = 0x50;                            /* CFR.W */
    hwwdg.Init.Counter = 0x7F;                           /* CR.T */
    hwwdg.Init.EWIMode = WWDG_EWI_ENABLE;                /* CFR.EWI = 1 */
    if (HAL_WWDG_Init(&hwwdg) != HAL_OK) Error_Handler();   /* CR.WDGA = 1 */
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

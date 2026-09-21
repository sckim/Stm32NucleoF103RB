/**
 * 07_IWDG_HAL_c  —  [계층: HAL]  독립 워치독(IWDG): 저속 내부 클럭(LSI)으로 도는 "마지막 안전장치"
 *
 * 02_WWDG 와의 비교
 *   WWDG : APB1 클럭 기반, "창(window)" 안에서만 리프레시 허용 (너무 빠른 리프레시도 리셋), 조기 경고 인터럽트, 짧은 타임아웃(ms).
 *   IWDG : **독립 저속 오실레이터 LSI(약 40kHz)** 로 동작 -> 메인 클럭이 죽어도(HSE/PLL 장애) 살아 있다. 타임아웃 전에 리프레시하면 되고
 *          (창 없음), 초 단위 타임아웃 가능. 한 번 켜면 **리셋 외에는 끌 수 없다**(LSI 도 자동으로 켜진다).
 *
 * 타임아웃 계산 (RM0008 19장)
 *   t = (reload + 1) x prescaler / f_LSI      (f_LSI 는 30~60kHz 로 개체/온도에 따라 크게 다르다, 공칭 40kHz)
 *   이 예제: prescaler = 64, reload = 624 -> 625 x 64 / 40000 = 1.0 s (공칭).  실제로는 0.67 ~ 1.33 s 범위.
 *   => IWDG 는 "정확한 시간"이 아니라 "이 정도 시간 안에 응답이 없으면 리셋"하는 용도이므로, 리프레시 주기를 타임아웃의 절반 이하(0.5s)로 잡는다.
 *   IWDG->KR 키: 0xCCCC 시작, 0x5555 설정 레지스터 접근 허용, 0xAAAA 리프레시(reload).  PR, RLR, SR(PVU/RVU 갱신 중 표시).
 *
 * 동작
 *   - 부팅 시 리셋 원인 출력 (RCC->CSR.IWDGRSTF -> "IWDG reset"). LD2 점등 1초로 부팅을 알린 뒤 워치독 시작.
 *   - 0.5초마다 HAL_IWDG_Refresh() 하고 LD2 토글 (정상)
 *   - B1(PC13)을 누르면 **프로그램이 멈춘 것을 흉내**(무한 루프, 리프레시 중단) -> 약 1초 뒤 IWDG 가 리셋 -> 부팅 메시지에 "IWDG reset"
 *   - 디버거로 정지 중 워치독이 리셋하지 않도록 __HAL_DBGMCU_FREEZE_IWDG() 로 디버그 중 정지시킨다.
 *
 * 리셋 원인 플래그 (RCC->CSR): IWDGRSTF(bit29), WWDGRSTF(bit30), SFTRSTF(bit28), PORRSTF(bit27), PINRSTF(bit26), LPWRRSTF(bit31)
 *   RMVF(bit24)에 1 을 써서 지운다.
 *
 * ISR: 인터럽트를 사용하지 않는다 (IWDG 는 인터럽트 없이 리셋만 한다).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

IWDG_HandleTypeDef hiwdg;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 리셋 원인 (워치독을 켜기 전에 읽고 지운다) */
    char msg[96];
    int n = snprintf(msg, sizeof msg, "\r\n[boot] reset cause:%s%s%s%s%s\r\n",
                     __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? " IWDG" : "",
                     __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? " WWDG" : "",
                     __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)  ? " SOFTWARE" : "",
                     __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)  ? " POWER-ON" : "",
                     __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)  ? " NRST-PIN" : "");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) print("*** the previous run was reset by the independent watchdog ***\r\n");
    __HAL_RCC_CLEAR_RESET_FLAGS();                                            /* CSR.RMVF = 1 */

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(1000);                                                          /* 부팅 표시 (워치독 시작 전이라 길어도 안전) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    __HAL_DBGMCU_FREEZE_IWDG();                                               /* 디버거 중단점에서 카운터 정지 */
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;                                 /* IWDG->PR : 40kHz/64 = 625Hz */
    hiwdg.Init.Reload = 624;                                                  /* IWDG->RLR : (624+1)/625Hz = 1.0s */
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) Error_Handler();                     /* KR=0xCCCC 로 시작 (이후 끌 수 없음) */
    print("watchdog started: timeout ~1s (0.67~1.33s by LSI tolerance), refresh every 0.5s, press B1 to simulate a hang\r\n");

    uint32_t t_refresh = HAL_GetTick();
    while (1)
    {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
        {
            print("B1: simulating a hang (no more refresh) -> reset in ~1s\r\n");
            while (1) {}                                                      /* 멈춘 프로그램: 리프레시 없음 */
        }

        if (HAL_GetTick() - t_refresh >= 500)
        {
            t_refresh += 500;
            HAL_IWDG_Refresh(&hiwdg);                                         /* IWDG->KR = 0xAAAA */
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;   /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;                                                          /* B1 */
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

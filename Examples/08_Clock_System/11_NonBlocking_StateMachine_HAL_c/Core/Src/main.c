/**
 * 11_NonBlocking_StateMachine_HAL_c  —  [계층: HAL]  HAL_Delay 없이 상태 머신(FSM)으로 만든 보행자 신호등
 *
 * 왜 상태 머신인가
 *   HAL_Delay() 로 순서대로 쓰면 그 시간 동안 버튼 입력·통신 등 다른 일을 전혀 못 한다.
 *   "지금 상태"와 "상태가 시작된 시각"만 기억하고, 루프를 계속 돌리며 (a) 시간 경과 (b) 버튼 이벤트를 확인해 상태를 바꾸면
 *   여러 일이 겹쳐도 반응성이 유지된다. (delay 탈출)
 *
 * 신호등 시나리오 (LED 3개)
 *   G(녹색) 5초 -> Y(황색) 1초 -> R(적색) 3초 -> G ...
 *   보행자 버튼(B1): 녹색 상태에서 최소 2초가 지난 뒤 누르면 남은 녹색을 건너뛰고 바로 황색으로 (녹색 최소 보장 시간).
 *   황색/적색 중 누른 경우는 요청만 기억했다가 다음 녹색이 최소 시간을 채운 뒤 즉시 황색으로 전환.
 *
 * 핀 (Nucleo-F103RB) : 녹색 = LD2(PA5), 황색 = PB0 ('A3'), 적색 = PB1 (Morpho CN10). 외부 LED 는 330Ω 저항과 직렬로 GND 로.
 *
 * 구현 요점
 *   - 상태 = enum, 전환 조건 = 시간(HAL_GetTick 차이, 오버플로 안전) 또는 이벤트
 *   - 진입 동작(entry action)은 전환 함수 한 곳에서만 수행 -> 상태가 바뀔 때 LED/로그가 항상 일관됨
 *   - 버튼은 5ms 마다 샘플링하는 "연속 N회 일치" 디바운스, 눌림 "에지"에서만 이벤트 발생
 *   - 상태 전환 로그를 UART(115200)로 출력 (시간 표시). 로그 출력 중에도 폴링 주기가 크게 흔들리지 않도록 짧은 문자열만 사용
 *
 * 인터럽트를 사용하지 않는다 (SysTick 이 HAL_GetTick 을 위해 돌고 있을 뿐).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

typedef enum { ST_GREEN, ST_YELLOW, ST_RED } state_t;

#define T_GREEN_MS      5000U
#define T_GREEN_MIN_MS  2000U
#define T_YELLOW_MS     1000U
#define T_RED_MS        3000U
#define DEBOUNCE_N      4U          /* 5ms x 4 = 20ms 연속 일치 */

UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

static state_t state;
static uint32_t state_since;
static uint8_t ped_request;

static const char *const state_name[] = { "GREEN", "YELLOW", "RED" };

static void set_leds(state_t s)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, s == ST_GREEN  ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, s == ST_YELLOW ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, s == ST_RED    ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 상태 전환의 유일한 통로: 진입 동작(LED, 시각, 로그)을 한 곳에 모은다 */
static void enter_state(state_t next, const char *reason)
{
    state = next;
    state_since = HAL_GetTick();
    set_leds(next);

    char m[64];
    int n = snprintf(m, sizeof m, "[%6lu ms] -> %-6s (%s)\r\n", (unsigned long)state_since, state_name[next], reason);
    HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 50);
}

/* 5ms 마다 호출: 연속 N회 같은 값이면 확정, 눌림 에지에서 1 반환 */
static int button_pressed_edge(void)
{
    static uint8_t cnt, stable = 1, last_raw = 1;
    uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);   /* 1 = 뗌, 0 = 눌림 */
    if (raw == last_raw) { if (cnt < DEBOUNCE_N) cnt++; } else { cnt = 0; last_raw = raw; }

    if (cnt >= DEBOUNCE_N && raw != stable)
    {
        stable = raw;
        if (stable == 0) return 1;                     /* 눌림 에지 */
    }
    return 0;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n[FSM traffic light] B1 = pedestrian request\r\n", 47, 100);
    enter_state(ST_GREEN, "boot");

    uint32_t t_key = HAL_GetTick();
    while (1)
    {
        uint32_t now = HAL_GetTick();
        uint32_t in_state = now - state_since;         /* 부호 없는 뺄셈: 49일 래핑에도 안전 */

        /* ---- 이벤트 수집 (5ms 주기) ---- */
        if (now - t_key >= 5)
        {
            t_key += 5;
            if (button_pressed_edge()) ped_request = 1;
        }

        /* ---- 상태별 전환 규칙 ---- */
        switch (state)
        {
        case ST_GREEN:
            if (ped_request && in_state >= T_GREEN_MIN_MS) { ped_request = 0; enter_state(ST_YELLOW, "pedestrian request"); }
            else if (in_state >= T_GREEN_MS)               {                  enter_state(ST_YELLOW, "timeout"); }
            break;
        case ST_YELLOW:
            if (in_state >= T_YELLOW_MS) enter_state(ST_RED, "timeout");
            break;
        case ST_RED:
            if (in_state >= T_RED_MS) enter_state(ST_GREEN, "timeout");
            break;
        }
        /* 이 자리에 다른 논블로킹 작업을 자유롭게 추가할 수 있다 (센서 읽기, 통신 처리 등) */
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;   /* 녹색 LD2 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;                                                                         /* 황색, 적색 */
    HAL_GPIO_Init(GPIOB, &g);
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

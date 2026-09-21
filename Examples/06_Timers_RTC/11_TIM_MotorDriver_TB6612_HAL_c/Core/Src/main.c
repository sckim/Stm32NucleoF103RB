/**
 * 11_TIM_MotorDriver_TB6612_HAL_c  —  [계층: HAL]  H-브리지 모터 드라이버(TB6612FNG)로 DC 모터: PWM 속도 + 방향 핀 + 정지 방식
 *
 * TB6612FNG 채널 A 제어 (PWMA, AIN1, AIN2, STBY)
 *   AIN1 AIN2 | 동작
 *    H    L   | 정회전 (PWMA 듀티가 속도)
 *    L    H   | 역회전
 *    H    H   | **쇼트 브레이크**: 모터 단자를 서로 단락 -> 빠르게 멈춘다 (역기전력이 모터를 제동)
 *    L    L   | **코스트(정지, 자유 회전)**: 출력 Hi-Z -> 관성으로 서서히 멈춘다
 *   STBY = Low 이면 전체 대기(출력 차단), High 여야 동작.
 *   PWM 주파수: 20kHz 로 가청 대역(약 20kHz) 위로 올려 모터 "삐-" 소리를 피한다. (TB6612 최대 100kHz)
 *
 * 배선 (Nucleo-F103RB)
 *   PWMA = PA0 (TIM2_CH1),  AIN1 = PB0,  AIN2 = PB1,  STBY = PB2
 *   VM = 모터 전원(4.5~13.5V, **외부 전원**),  VCC = 3.3V(로직),  GND = 보드 GND 와 **공통**,  AO1/AO2 = 모터
 *   *** 모터 전원을 Nucleo 에서 끌어 쓰지 말 것. 기동 전류가 보드를 리셋시킨다. 모터 양단에 역기전력 흡수를 위한 캐패시터/다이오드 권장. ***
 *
 * PWM 계산: TIM2 64MHz, PSC = 0, ARR = 3200-1 -> 20kHz, CCR1 = 0..3200 (속도 -1000..+1000 의 절댓값을 0..3200 으로 스케일)
 *
 * 동작 시퀀스 (반복, UART 로 단계 출력)
 *   1) 정회전 가속 0 -> 100% (2초)   2) 쇼트 브레이크 0.5초   3) 역회전 가속 0 -> 100% (2초)   4) 코스트 1초(자연 정지)
 *   B1(PC13)을 누르면 즉시 **브레이크 + STBY Low**(비상 정지). 다시 누르면 재개.
 *   *** 이 예제는 단계마다 급격한 방향 전환을 하지 않는다: 반드시 속도를 0 으로 줄이거나 브레이크를 거쳐 방향을 바꾼다
 *       (달리는 모터에 즉시 반대 방향 전압을 주면 큰 전류가 흘러 드라이버/전원에 무리가 간다). ***
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define PWM_TOP 3200U

TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_PWM_Init(void);

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

static void set_dir(int in1, int in2)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, in1 ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* AIN1 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, in2 ? GPIO_PIN_SET : GPIO_PIN_RESET);   /* AIN2 */
}

/* speed: -1000(역 최대) .. 0 .. +1000(정 최대) */
static void motor_set(int speed)
{
    if (speed > 1000) speed = 1000;
    if (speed < -1000) speed = -1000;
    if (speed > 0)       set_dir(1, 0);
    else if (speed < 0)  set_dir(0, 1);
    else                 set_dir(0, 0);                                      /* 0 은 코스트 */
    uint32_t mag = (uint32_t)(speed < 0 ? -speed : speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, mag * PWM_TOP / 1000U);      /* TIM2->CCR1 */
}

static void motor_brake(void)   { __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, PWM_TOP); set_dir(1, 1); }   /* 쇼트 브레이크 (PWM 100%) */
static void motor_coast(void)   { __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); set_dir(0, 0); }          /* 자유 회전 */
static void standby(int on)     { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, on ? GPIO_PIN_RESET : GPIO_PIN_SET); }  /* STBY Low = 대기 */

/* 지정 시간 동안 대기하면서 B1 비상 정지를 감시. 눌렸으면 1 반환 */
static int wait_or_estop(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < ms)
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) return 1;
    return 0;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM2_PWM_Init();

    standby(0);                                                              /* 출력 활성 (STBY High) */
    motor_coast();

    while (1)
    {
        int estop = 0;

        print("forward ramp 0 -> 100%\r\n");
        for (int s = 0; s <= 1000 && !estop; s += 10) { motor_set(s); estop = wait_or_estop(20); }

        if (!estop) { print("short brake\r\n"); motor_brake(); estop = wait_or_estop(500); }

        if (!estop) { print("reverse ramp 0 -> 100%\r\n"); for (int s = 0; s <= 1000 && !estop; s += 10) { motor_set(-s); estop = wait_or_estop(20); } }

        if (!estop) { print("coast (free spin)\r\n"); motor_coast(); estop = wait_or_estop(1000); }

        if (estop)
        {
            motor_brake();
            HAL_Delay(300);
            standby(1);                                                      /* STBY Low: 출력 차단 */
            print("*** E-STOP: brake + standby. release B1 and press again to resume\r\n");
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}   /* 뗄 때까지 */
            HAL_Delay(50);
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {}     /* 다시 누를 때까지 */
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
            HAL_Delay(50);
            standby(0);
            motor_coast();
            print("resume\r\n");
        }
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
}

static void MX_TIM2_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    TIM_OC_InitTypeDef oc = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_0;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* PWMA = TIM2_CH1 */
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = PWM_TOP - 1;                    /* 64MHz / 3200 = 20kHz */
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();
    oc.OCMode = TIM_OCMODE_PWM1;  oc.Pulse = 0;  oc.OCPolarity = TIM_OCPOLARITY_HIGH;  oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);                    /* STBY Low 로 시작(안전) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;                            /* AIN1, AIN2, STBY */
    g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_13;  g.Mode = GPIO_MODE_INPUT;                          /* B1 */
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

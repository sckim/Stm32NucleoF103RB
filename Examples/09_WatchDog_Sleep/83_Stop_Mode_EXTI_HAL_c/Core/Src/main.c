/**
 * 83_Stop_Mode_EXTI_HAL_c  —  [계층: HAL]  Stop 모드 진입과 EXTI(버튼) 웨이크업
 *
 * 동작
 *   1) 부팅 후 LD2 를 켜고 3초간 "실행 중" 메시지 출력
 *   2) LD2 를 끄고 Stop 모드 진입 (고속 클럭/PLL 정지, SRAM·레지스터 값 유지, 전류 수 uA 수준)
 *   3) B1(PC13) 을 누르면 EXTI13 하강 에지로 깨어남 -> 클럭 재설정 -> "wake" 메시지 -> 1로 돌아가 반복
 *
 * Stop 모드 핵심 (RM0008 5장)
 *   PWR->CR.PDDS = 0, SCB->SCR.SLEEPDEEP = 1 후 WFI  ->  HAL_PWR_EnterSTOPMode() 가 수행
 *   PWR->CR.LPDS = 1 : 전압 레귤레이터를 저전력으로 (전류 더 감소, 웨이크업이 조금 느림)
 *   웨이크업 원인: 임의의 EXTI 라인(0~15), RTC 알람(EXTI17), USB 웨이크업 등
 *   *** 깨어난 직후 시스템 클럭은 HSI 8MHz 로 되돌아가 있다 -> SystemClock_Config() 를 다시 호출해야 PLL 64MHz 복구 ***
 *   Stop 중에는 SysTick 도 정지하지만, 진입 전에 HAL_SuspendTick() 으로 SysTick 인터럽트를 꺼 두지 않으면
 *   진입 직후 SysTick 인터럽트에 바로 깨어난다.
 *
 * ISR vs 콜백
 *   EXTI15_10_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13)
 *     -> HAL_GPIO_EXTI_Callback() (이 파일) : 웨이크업 원인 표시용 플래그만 설정
 *
 * 디버깅 주의: Stop 중 디버거 유지는 DBGMCU_CR.DBG_STOP=1 (아래에서 설정). 전류 측정 시에는 디버거를 뽑고 잰다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

static volatile uint8_t woke_by_button;

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

    HAL_DBGMCU_EnableDBGStopMode();                   /* DBGMCU->CR.DBG_STOP = 1 */
    print("\r\n[Stop mode demo] press B1 to wake\r\n");

    while (1)
    {
        /* 1) 실행 구간 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        for (int s = 1; s <= 3; s++)
        {
            char m[40];
            int n = snprintf(m, sizeof m, "running... %d\r\n", s);
            HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 100);
            HAL_Delay(1000);
        }

        /* 2) Stop 진입 준비 */
        print("entering STOP (SYSCLK/PLL off)\r\n");    /* HAL_UART_Transmit 은 TC(전송 완료)까지 기다린다 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        woke_by_button = 0;
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);        /* 남은 EXTI 펜딩 제거 (EXTI->PR bit13) */

        HAL_SuspendTick();                            /* SysTick 인터럽트 끄기 (SysTick->CTRL.TICKINT = 0) */
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);   /* --- 여기서 잠든다 --- */

        /* 3) 깨어남: 클럭이 HSI 로 되돌아가 있으므로 복구 */
        SystemClock_Config();
        HAL_ResumeTick();

        print(woke_by_button ? "wake: EXTI13 (B1)\r\n" : "wake: other\r\n");
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}   /* 버튼 뗄 때까지 (채터링 방지) */
        HAL_Delay(50);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): EXTI 라인 인터럽트 처리 후 HAL 이 호출                    */
/* ------------------------------------------------------------------------- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13) woke_by_button = 1;
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();                      /* EXTI 라인 -> 포트 선택은 AFIO->EXTICR 에서 한다 */

    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;  /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_13;                              /* B1: 하강 에지 EXTI  (AFIO->EXTICR[3] = 포트 C, EXTI->FTSR bit13) */
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
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

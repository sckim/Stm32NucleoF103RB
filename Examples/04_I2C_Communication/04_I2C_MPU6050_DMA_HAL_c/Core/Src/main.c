/**
 * 04_I2C_MPU6050_DMA_HAL_c  —  [계층: HAL]  DMA 로 하는 논블로킹 I2C: 전송 중에도 CPU 는 다른 일을 한다
 *
 * 03_I2C_MPU6050 은 HAL_I2C_Mem_Read() 가 전송이 끝날 때까지(약 1.4ms @100kHz) CPU 를 붙잡는다.
 * 이 예제는 HAL_I2C_Mem_Read_DMA() 로 읽기를 "시작"만 하고 곧바로 다른 일을 하다가, 완료 콜백에서 결과를 받는다.
 *
 * 배선: MPU6050(GY-521) VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND (주소 0x68) — 03_I2C_MPU6050 과 동일
 *
 * 동작
 *   - 100ms 마다 0x3B 부터 14바이트를 DMA 로 읽기 시작. 그동안 main 은 "다른 일"(free_loops 카운터)을 계속 수행.
 *   - 완료 콜백이 raw 데이터를 복사하고 플래그를 세움 -> main 이 변환해 UART(115200)로 출력.
 *   - 출력에는 "전송 시작~완료 사이에 main 이 돈 루프 수"가 함께 표시되어 CPU 가 자유로웠음을 보여준다.
 *   - 버스 오류(NACK, 타임아웃)는 ErrorCallback 에서 플래그로 알리고, main 이 I2C 를 재초기화해 복구한다.
 *
 * DMA 채널 매핑 (RM0008 13.3.7): I2C1_TX -> DMA1 채널6,  I2C1_RX -> DMA1 채널7
 *   I2C1->CR2.DMAEN=1, LAST=1 (마지막 바이트에서 NACK + STOP 자동 생성)
 *
 * ISR vs 콜백
 *   DMA1_Channel7_IRQHandler / DMA1_Channel6_IRQHandler / I2C1_EV_IRQHandler / I2C1_ER_IRQHandler (stm32f1xx_it.c, 진짜 ISR)
 *     -> HAL_DMA_IRQHandler / HAL_I2C_EV_IRQHandler / HAL_I2C_ER_IRQHandler
 *       -> HAL_I2C_MemRxCpltCallback() (수신 완료), HAL_I2C_ErrorCallback() (오류)  (이 파일)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define MPU_ADDR      (0x68 << 1)
#define REG_WHO_AM_I  0x75
#define REG_PWR_MGMT1 0x6B
#define REG_ACCEL_OUT 0x3B

I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;
UART_HandleTypeDef huart2;

static uint8_t dma_buf[14];                     /* DMA 가 채우는 버퍼 */
static uint8_t data[14];                        /* 콜백이 복사해 둔 사본 */
static volatile uint8_t rx_done, rx_error;
static volatile uint32_t loops_during;          /* 전송 중 main 이 돈 횟수 */

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    char msg[128];
    uint8_t who = 0, wake = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)   /* 초기화는 블로킹 API 로 */
    {
        print("\r\nMPU6050 not responding\r\n");
        Error_Handler();
    }
    HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, REG_PWR_MGMT1, I2C_MEMADD_SIZE_8BIT, &wake, 1, 100);   /* 슬립 해제 */
    snprintf(msg, sizeof msg, "\r\nWHO_AM_I=0x%02X, starting DMA reads\r\n", who);
    print(msg);

    uint32_t t_read = HAL_GetTick();
    uint32_t loops = 0;
    uint8_t busy = 0;
    while (1)
    {
        loops++;                                               /* "다른 일" 흉내 */

        if (!busy && HAL_GetTick() - t_read >= 100)
        {
            t_read += 100;
            rx_done = 0; rx_error = 0; loops = 0;
            if (HAL_I2C_Mem_Read_DMA(&hi2c1, MPU_ADDR, REG_ACCEL_OUT, I2C_MEMADD_SIZE_8BIT, dma_buf, 14) == HAL_OK)
                busy = 1;                                      /* 시작만 하고 즉시 반환 */
        }

        if (busy && rx_done)
        {
            busy = 0;
            loops_during = loops;
            int16_t ax = (int16_t)((data[0] << 8) | data[1]);
            int16_t ay = (int16_t)((data[2] << 8) | data[3]);
            int16_t az = (int16_t)((data[4] << 8) | data[5]);
            int16_t gz = (int16_t)((data[12] << 8) | data[13]);
            snprintf(msg, sizeof msg, "acc[mg] %6ld %6ld %6ld | gyroZ[0.1dps] %6ld | main loops during DMA = %lu\r\n",
                     (long)ax * 1000 / 16384, (long)ay * 1000 / 16384, (long)az * 1000 / 16384,
                     (long)gz * 10 / 131, (unsigned long)loops_during);
            print(msg);
        }
        else if (busy && rx_error)
        {
            busy = 0;
            print("I2C error -> re-init\r\n");
            HAL_I2C_DeInit(&hi2c1);
            MX_I2C1_Init();
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥)                                                          */
/* ------------------------------------------------------------------------- */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) { memcpy(data, dma_buf, sizeof data); rx_done = 1; }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) rx_error = 1;
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_OD;  g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    hdma_i2c1_rx.Instance = DMA1_Channel7;                    /* I2C1_RX */
    hdma_i2c1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2c1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c1_rx.Init.Mode = DMA_NORMAL;
    hdma_i2c1_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_i2c1_rx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hi2c, hdmarx, hdma_i2c1_rx);

    hdma_i2c1_tx.Instance = DMA1_Channel6;                    /* I2C1_TX (레지스터 주소 전송 단계는 인터럽트로 처리) */
    hdma_i2c1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2c1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c1_tx.Init.Mode = DMA_NORMAL;
    hdma_i2c1_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_i2c1_tx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(hi2c, hdmatx, hdma_i2c1_tx);

    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 0);  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 0);  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);   /* 주소/레지스터 단계 이벤트 */
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);   /* NACK, 버스 오류 */
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
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

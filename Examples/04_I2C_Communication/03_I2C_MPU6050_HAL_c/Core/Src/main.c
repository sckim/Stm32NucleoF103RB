/**
 * 03_I2C_MPU6050_HAL_c  —  [계층: HAL]  I2C 레지스터 읽기로 MPU6050 가속도/자이로/온도 읽기
 *
 * 배선: MPU6050(GY-521) VCC=3.3V, GND, SCL=PB6, SDA=PB7, AD0=GND (7비트 주소 0x68)
 *
 * 동작
 *   1) WHO_AM_I(0x75) 를 읽어 0x68 인지 확인
 *   2) PWR_MGMT_1(0x6B)=0 으로 슬립 해제 (리셋 직후 MPU6050 은 슬립 상태)
 *   3) 200ms 마다 0x3B 부터 14바이트(ACCEL XYZ, TEMP, GYRO XYZ)를 한 번에 읽어 UART(115200)로 출력
 *
 * 단위 변환 (기본 범위: 가속도 ±2g, 자이로 ±250°/s)
 *   가속도: 16384 LSB/g  -> mg = raw x 1000 / 16384
 *   자이로: 131 LSB/(°/s) -> 0.1°/s = raw x 10 / 131
 *   온도  : T[°C] = raw/340 + 36.53  -> 0.01°C 단위 = raw x 100 / 340 + 3653   (부동소수점 없이 정수 연산)
 *
 * I2C 메모리 읽기 시퀀스 (HAL_I2C_Mem_Read)
 *   START - [0x68<<1|W] - [레지스터 주소] - RESTART - [0x68<<1|R] - 데이터 N바이트(마지막 NACK) - STOP
 *
 * 인터럽트를 사용하지 않는다 (폴링). 비동기로 바꾸려면 HAL_I2C_Mem_Read_IT/_DMA 와
 * HAL_I2C_MemRxCpltCallback 을 사용한다 (ISR: I2C1_EV_IRQHandler/I2C1_ER_IRQHandler -> HAL_I2C_EV/ER_IRQHandler).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define MPU_ADDR      (0x68 << 1)   /* HAL 은 7비트 주소를 왼쪽으로 1비트 민 값을 요구 */
#define REG_WHO_AM_I  0x75
#define REG_PWR_MGMT1 0x6B
#define REG_ACCEL_OUT 0x3B

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

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
    uint8_t who = 0;

    if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)
    {
        print("\r\nMPU6050 not responding (check wiring / pull-ups / AD0)\r\n");
        Error_Handler();
    }
    snprintf(msg, sizeof msg, "\r\nWHO_AM_I = 0x%02X (expect 0x68)\r\n", who);
    print(msg);

    uint8_t wake = 0x00;                                   /* SLEEP=0, 내부 8MHz 클럭 */
    if (HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, REG_PWR_MGMT1, I2C_MEMADD_SIZE_8BIT, &wake, 1, 100) != HAL_OK) Error_Handler();

    while (1)
    {
        uint8_t b[14];
        if (HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, REG_ACCEL_OUT, I2C_MEMADD_SIZE_8BIT, b, 14, 100) == HAL_OK)
        {
            /* 상위 바이트 먼저(big-endian) 로 도착 -> int16_t 로 조립 */
            int16_t ax = (int16_t)((b[0] << 8) | b[1]);
            int16_t ay = (int16_t)((b[2] << 8) | b[3]);
            int16_t az = (int16_t)((b[4] << 8) | b[5]);
            int16_t tp = (int16_t)((b[6] << 8) | b[7]);
            int16_t gx = (int16_t)((b[8] << 8) | b[9]);
            int16_t gy = (int16_t)((b[10] << 8) | b[11]);
            int16_t gz = (int16_t)((b[12] << 8) | b[13]);

            int32_t t100 = (int32_t)tp * 100 / 340 + 3653;   /* 0.01 °C */
            snprintf(msg, sizeof msg,
                     "acc[mg] %6ld %6ld %6ld | gyro[0.1dps] %6ld %6ld %6ld | temp %ld.%02ld C\r\n",
                     (long)ax * 1000 / 16384, (long)ay * 1000 / 16384, (long)az * 1000 / 16384,
                     (long)gx * 10 / 131, (long)gy * 10 / 131, (long)gz * 10 / 131,
                     (long)(t100 / 100), (long)(t100 % 100));
            print(msg);
        }
        else
        {
            print("read error\r\n");
            HAL_I2C_DeInit(&hi2c1);                        /* 버스 오류 복구: 재초기화 */
            MX_I2C1_Init();
        }
        HAL_Delay(200);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
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

/**
 * 31_I2C_Scan_HAL_c  —  [계층: HAL]  I2C1 버스 스캐너 (연결된 슬레이브 주소 찾기)
 *
 * 동작
 *   - 7비트 주소 0x08~0x77 을 하나씩 "주소 + 쓰기" 로 호출하고, 슬레이브가 ACK 하면 그 주소를 출력한다.
 *   - 2초마다 다시 스캔하여 결과를 UART(115200)로 출력. 흔한 장치는 이름을 함께 표시.
 *
 * 배선 (Nucleo-F103RB, 리맵 없는 기본 핀)
 *   I2C1_SCL = PB6 ('D10'),  I2C1_SDA = PB7 (Morpho CN7 핀21)
 *   SCL/SDA 각각 4.7kΩ 풀업(3.3V) 필요 — 대부분의 모듈(PCF8574, MPU6050 등)에는 이미 장착
 *
 * 하드웨어 대응 (RM0008 26장)
 *   I2C1->CR2.FREQ = APB1 클럭(MHz), I2C1->CCR/TRISE = 100kHz 표준 모드 타이밍
 *   주소 호출: CR1.START -> DR = (addr<<1 | 0) -> SR1.ADDR(ACK) 또는 SR1.AF(NACK) -> CR1.STOP
 *   HAL_I2C_IsDeviceReady() 가 위 절차를 수행한다 (HAL 주소 인자는 "7비트 주소를 1비트 왼쪽 시프트한 값").
 *
 * 인터럽트를 사용하지 않는다 (폴링). 버스에 풀업이 없으면 BUSY 상태에 걸려 결과가 나오지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

static const char *guess_name(uint8_t a)
{
    switch (a)
    {
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26:
    case 0x27: return "PCF8574 (A=000..111) / LCD 백팩";
    case 0x38: case 0x39: case 0x3A: case 0x3B: return "PCF8574A";
    case 0x3C: case 0x3D: return "SSD1306 OLED";
    case 0x3F: return "PCF8574A LCD 백팩";
    case 0x48: return "ADS1115 / TMP102";
    case 0x50: case 0x57: return "AT24Cxx EEPROM";
    case 0x68: return "MPU6050 / DS1307 / DS3231";
    case 0x76: case 0x77: return "BMP280 / BME280";
    default: return "";
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    char msg[80];
    while (1)
    {
        const char *h = "\r\n--- I2C1 scan (0x08..0x77) ---\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)h, (uint16_t)strlen(h), 100);

        int found = 0;
        for (uint8_t addr = 0x08; addr <= 0x77; addr++)
        {
            /* 1회 시도, 타임아웃 10ms. HAL_OK = ACK 수신 */
            if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 10) == HAL_OK)
            {
                int n = snprintf(msg, sizeof msg, "found 0x%02X  %s\r\n", addr, guess_name(addr));
                HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
                found++;
            }
        }
        int n = snprintf(msg, sizeof msg, "%d device(s)\r\n", found);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(2000);
    }
}

/* HAL_I2C_Init 이 호출: 클럭/핀 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();            /* RCC->APB1ENR.I2C1EN */
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;               /* 대체기능 오픈드레인 (CNF=11): I2C 는 반드시 오픈드레인 */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;                     /* 표준 모드 100kHz */
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

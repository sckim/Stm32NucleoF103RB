/**
 * 07_I2C_DS3231_RTC_HAL_c  —  [계층: HAL]  I2C 외장 RTC DS3231: BCD 시각 읽기/쓰기, 온도 센서, 컴파일 시각으로 설정
 *
 * F103 내부 RTC(05_RTC_Calendar)는 크리스털 정확도와 VBAT 에 의존한다. DS3231 은 온도 보상 크리스털(TCXO)을 내장해
 * 연 ±수 분 이하로 정확하고, 코인 셀로 전원이 꺼져도 시간을 유지하는 외장 RTC 모듈이다 (모듈: ZS-042, DS3231 보드 등).
 *
 * 배선: VCC=3.3V(5V 도 가능한 모듈이 많다, 풀업 전압을 확인), GND, SCL=PB6, SDA=PB7.  7비트 주소 0x68.
 *
 * DS3231 레지스터 맵 (BCD 형식!)
 *   0x00 초, 0x01 분, 0x02 시(24시간제), 0x03 요일(1~7), 0x04 일, 0x05 월(bit7 = 세기), 0x06 년(00~99)
 *   0x0E 제어, 0x0F 상태(OSF = 오실레이터 정지 플래그: 전원이 완전히 끊겼었다는 표시),
 *   0x11/0x12 온도: MSB = 정수부(부호 있음), LSB 상위 2비트 = 소수부 (0.25°C 단위)
 *   BCD: 십의 자리가 상위 니블, 일의 자리가 하위 니블  (예: 59초 = 0x59).  bcd2bin(x) = (x>>4)*10 + (x&0x0F)
 *
 * 동작
 *   - 부팅 시 OSF 플래그가 1 이거나 B1(PC13)을 누른 채 리셋하면 시각을 "컴파일 시각(__DATE__/__TIME__)"으로 설정
 *   - 1초마다 "YYYY-MM-DD HH:MM:SS (요일) temp=xx.xx C" 를 UART(115200)로 출력
 *   - 전원을 껐다 켜도(코인 셀 있을 때) 시각이 이어지는 것을 확인
 *
 * 참고: 같은 I2C 버스에 EEPROM(06_I2C_EEPROM_AT24C, ZS-042 는 0x57)이 함께 있을 수 있다 (02_I2C_Scan 으로 확인).
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define RTC_ADDR (0x68 << 1)

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

/* 컴파일 시각 파싱: __DATE__ = "Sep 22 2026", __TIME__ = "12:34:56" */
static void parse_compile_time(uint8_t t[7])
{
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__, *tm = __TIME__;
    int mo = 1;
    for (int i = 0; i < 12; i++) if (strncmp(d, &mon[i * 3], 3) == 0) { mo = i + 1; break; }
    int day = (d[4] == ' ') ? (d[5] - '0') : ((d[4] - '0') * 10 + (d[5] - '0'));
    int year = (d[9] - '0') * 10 + (d[10] - '0');                       /* 2자리 연도 */
    int y4 = 2000 + year;
    int h = (tm[0] - '0') * 10 + (tm[1] - '0'), mi = (tm[3] - '0') * 10 + (tm[4] - '0'), s = (tm[6] - '0') * 10 + (tm[7] - '0');
    /* 요일(1~7, 일요일=1 로 사용): Zeller 계열 간이 계산 (Sakamoto) */
    static const int tt[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int y = y4 - (mo < 3);
    int wd = (y + y / 4 - y / 100 + y / 400 + tt[mo - 1] + day) % 7;   /* 0=일요일 */
    t[0] = bin2bcd((uint8_t)s);  t[1] = bin2bcd((uint8_t)mi);  t[2] = bin2bcd((uint8_t)h);
    t[3] = bin2bcd((uint8_t)(wd + 1));  t[4] = bin2bcd((uint8_t)day);  t[5] = bin2bcd((uint8_t)mo);  t[6] = bin2bcd((uint8_t)year);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    char msg[96];
    uint8_t status = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, RTC_ADDR, 0x0F, I2C_MEMADD_SIZE_8BIT, &status, 1, 100) != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\nDS3231 not found at 0x68\r\n", 28, 100);
        Error_Handler();
    }

    int force = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);    /* B1 를 누른 채 부팅 */
    if ((status & 0x80) || force)                                            /* OSF: 전원이 끊겼었음 / 강제 설정 */
    {
        uint8_t t[7];
        parse_compile_time(t);
        HAL_I2C_Mem_Write(&hi2c1, RTC_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, t, 7, 100);   /* 0x00~0x06 을 한 번에 기록 */
        status &= (uint8_t)~0x80;                                            /* OSF 플래그 삭제 */
        HAL_I2C_Mem_Write(&hi2c1, RTC_ADDR, 0x0F, I2C_MEMADD_SIZE_8BIT, &status, 1, 100);
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\ntime set from compile time\r\n", 30, 100);
    }

    static const char *const wd[8] = { "?", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    while (1)
    {
        uint8_t r[7], tp[2];
        if (HAL_I2C_Mem_Read(&hi2c1, RTC_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, r, 7, 100) == HAL_OK &&
            HAL_I2C_Mem_Read(&hi2c1, RTC_ADDR, 0x11, I2C_MEMADD_SIZE_8BIT, tp, 2, 100) == HAL_OK)
        {
            int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1] & 0x7F), hour = bcd2bin(r[2] & 0x3F);
            int dow = r[3] & 0x07, day = bcd2bin(r[4] & 0x3F), mon = bcd2bin(r[5] & 0x1F), year = 2000 + bcd2bin(r[6]);
            /* 온도: MSB 는 부호 있는 정수부, LSB 상위 2비트가 1/4 도 단위 */
            int t100 = ((int8_t)tp[0]) * 100 + (tp[1] >> 6) * 25;                /* 소수부는 항상 양수로 더한다 (-5.25 = -6 + 0.75) */
            int n = snprintf(msg, sizeof msg, "%04d-%02d-%02d %02d:%02d:%02d (%s)  temp=%d.%02d C\r\n",
                             year, mon, day, hour, min, sec, wd[dow <= 7 ? dow : 0], t100 / 100, (t100 < 0 ? -t100 : t100) % 100);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        }
        else
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)"i2c read error\r\n", 16, 100);
        }
        HAL_Delay(1000);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_OD;  g.Speed = GPIO_SPEED_FREQ_HIGH;
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

/**
 * 07_SPI_MAX7219_HAL_c  —  [계층: HAL]  MAX7219 로 8x8 LED 매트릭스 구동 (SPI 단방향 16비트 프레임)
 *
 * 배선 (MAX7219 8x8 매트릭스 모듈, 5V 모듈이 많다: VCC 를 5V 로, 로직 입력은 3.3V 로 대개 인식된다)
 *   VCC = 5V, GND
 *   CLK = PA5 (SPI1_SCK, LD2 와 공유),  DIN = PA7 (SPI1_MOSI),  CS(LOAD) = PA4
 *
 * MAX7219 규칙
 *   16비트 프레임 [레지스터 주소 8비트][데이터 8비트] 를 MSB 먼저 보내고, **CS(LOAD)를 High 로 올리는 순간** 반영된다.
 *   여러 개를 캐스케이드하면 프레임이 시프트 레지스터를 따라 다음 칩으로 밀려 가므로, 칩 수만큼 프레임을 연달아 보낸 뒤 CS 를 올린다.
 *   레지스터: 0x01~0x08 = 행(Digit0~7) 데이터 (8x8 매트릭스에서는 행 하나 = 비트 8개),  0x09 Decode(0=사용 안 함),
 *            0x0A 밝기(0~15),  0x0B Scan limit(7 = 8행 전부),  0x0C Shutdown(1=동작),  0x0F Display test(1=전체 점등)
 *   SPI: 모드 0, 최대 약 10MHz. 이 예제는 1MHz(64MHz/64)로도 충분하다.
 *
 * 동작 (일정 시간마다 전환)
 *   1) 전체 점등 테스트 0.5초  2) 행 스캔(줄이 위에서 아래로)  3) 튕기는 점  4) 확장하는 사각형 테두리  5) 밝기가 숨쉬듯 변화
 *   그림은 RAM 의 프레임 fb[8](행당 1바이트)에 그린 뒤 flush() 로 8행을 전송한다.
 *   (글자 표시는 문자별 8x8 비트맵 표를 fb 로 복사하고 옆으로 밀면 스크롤이 된다 — 표를 추가해 확장)
 *
 * 인터럽트를 사용하지 않는다 (블로킹 SPI).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

static uint8_t fb[8];                          /* 프레임버퍼: fb[row] 의 비트7 = 왼쪽 끝 열 */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);

static void max_write(uint8_t reg, uint8_t val)
{
    uint8_t frame[2] = { reg, val };
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, frame, 2, 10);
    CS_HIGH();                                  /* 이 상승 에지에서 레지스터에 반영 */
}

static void flush(void)
{
    for (uint8_t r = 0; r < 8; r++) max_write((uint8_t)(r + 1), fb[r]);
}

static void max_init(void)
{
    max_write(0x0F, 0x00);                      /* 디스플레이 테스트 끔 */
    max_write(0x0C, 0x00);                      /* 초기 셧다운 */
    max_write(0x09, 0x00);                      /* 디코드 모드 없음 (매트릭스는 원시 비트 사용) */
    max_write(0x0B, 0x07);                      /* 8행 스캔 */
    max_write(0x0A, 0x04);                      /* 밝기 4/15 */
    memset(fb, 0, sizeof fb);
    flush();
    max_write(0x0C, 0x01);                      /* 동작 시작 */
}

static void set_px(int x, int y, int on)
{
    if ((unsigned)x > 7 || (unsigned)y > 7) return;
    if (on) fb[y] |= (uint8_t)(0x80u >> x); else fb[y] &= (uint8_t)~(0x80u >> x);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();
    max_init();

    /* 1) 디스플레이 테스트: 전체 점등 0.5초 (모듈 연결 확인) */
    max_write(0x0F, 0x01);  HAL_Delay(500);  max_write(0x0F, 0x00);

    for (;;)
    {
        /* 2) 행 스캔 */
        for (int r = 0; r < 8; r++) { memset(fb, 0, sizeof fb); fb[r] = 0xFF; flush(); HAL_Delay(80); }

        /* 3) 튕기는 점 (약 4초) */
        int x = 1, y = 2, vx = 1, vy = 1;
        for (int i = 0; i < 80; i++)
        {
            memset(fb, 0, sizeof fb);
            x += vx; y += vy;
            if (x <= 0 || x >= 7) vx = -vx;
            if (y <= 0 || y >= 7) vy = -vy;
            set_px(x, y, 1);
            flush(); HAL_Delay(50);
        }

        /* 4) 안쪽에서 바깥으로 커지는 사각형 테두리, 3번 반복 */
        for (int rep = 0; rep < 3; rep++)
            for (int s = 0; s < 4; s++)
            {
                memset(fb, 0, sizeof fb);
                int lo = 3 - s, hi = 4 + s;
                for (int k = lo; k <= hi; k++) { set_px(k, lo, 1); set_px(k, hi, 1); set_px(lo, k, 1); set_px(hi, k, 1); }
                flush(); HAL_Delay(120);
            }

        /* 5) 밝기 페이드: 전체 점등 상태에서 0->15->0 */
        memset(fb, 0xFF, sizeof fb); flush();
        for (int b = 0; b <= 15; b++) { max_write(0x0A, (uint8_t)b); HAL_Delay(60); }
        for (int b = 15; b >= 0; b--) { max_write(0x0A, (uint8_t)b); HAL_Delay(60); }
        max_write(0x0A, 0x04);

        HAL_UART_Transmit(&huart2, (uint8_t *)"animation cycle done\r\n", 22, 100);
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;    /* SCK, MOSI */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_1LINE;                    /* 송신 전용 */
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;      /* 64MHz/64 = 1MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_4;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* CS(LOAD) */
    HAL_GPIO_Init(GPIOA, &g);
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

/**
 * 06_I2C_OLED_SSD1306_HAL_c  —  [계층: HAL]  I2C 로 SSD1306 128x64 OLED 구동: 프레임버퍼, 선/사각형, 숫자 표시, 애니메이션
 *
 * 배선: OLED 모듈(I2C 4핀) VCC=3.3V, GND, SCL=PB6, SDA=PB7   (7비트 주소 0x3C, HAL 에는 0x78 = 0x3C<<1 로 전달)
 *       모듈에 풀업이 없으면 SCL/SDA 각각 4.7kΩ 풀업 추가.
 *
 * SSD1306 구조
 *   128x64 픽셀 = 8 페이지(가로 줄) x 128 열. 1바이트 = 세로 8픽셀(LSB 가 위쪽). 화면 전체 = 1024바이트.
 *   I2C 프레임: [주소+W] [제어 바이트] [데이터...]   제어 바이트 0x00 = 뒤따르는 것이 "명령", 0x40 = 뒤따르는 것이 "표시 데이터"
 *   화면 램(GDDRAM)에 쓰기 전에 주소 지정 모드(수평, 0x20 0x00)와 열/페이지 범위(0x21, 0x22)를 설정해 두면
 *   1024바이트를 연속으로 밀어 넣는 것만으로 전체 화면이 갱신된다.
 *
 * 이 예제의 구조 (마이크로컨트롤러 RAM 에 프레임버퍼 1KB 를 두고 그림 -> 통째로 전송하는 정석 패턴)
 *   fb[1024] : 그리기 함수(px, hline, rect, line, text)는 fb 만 수정 -> oled_flush() 가 fb 를 OLED 로 전송
 *   글꼴: 공간 절약을 위해 3x5 픽셀 글꼴(숫자 0-9, ':', '.', '-', 공백, 'F', 'P', 'S')만 내장. 필요한 글자를 표에 추가해 확장.
 *   화면: 테두리 안에서 공이 튕기고, 프레임 번호와 FPS 를 숫자로 표시.
 *   I2C 400kHz(고속 모드)로 1024바이트 전송 = 약 23ms -> 최대 약 40 FPS (측정값이 화면과 UART 에 표시됨)
 *
 * 인터럽트를 사용하지 않는다 (블로킹 I2C). 부드러운 애니메이션과 CPU 절약이 필요하면 HAL_I2C_Mem_Write_DMA 로 전송(04_I2C_MPU6050_DMA 참고).
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define OLED_ADDR (0x3C << 1)
#define OLED_W 128
#define OLED_H 64

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

static uint8_t fb[OLED_W * OLED_H / 8];

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

/* ---------------- SSD1306 저수준 ---------------- */
static HAL_StatusTypeDef oled_cmds(const uint8_t *c, uint16_t n)
{
    return HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t *)c, n, 100);   /* 제어 바이트 0x00 = 명령 */
}

static HAL_StatusTypeDef oled_init(void)
{
    static const uint8_t init[] = {
        0xAE,             /* 디스플레이 끄기 */
        0xD5, 0x80,       /* 클럭 분주/오실레이터 주파수 */
        0xA8, 0x3F,       /* 멀티플렉스 비율 = 64 */
        0xD3, 0x00,       /* 디스플레이 오프셋 = 0 */
        0x40,             /* 시작 라인 = 0 */
        0x8D, 0x14,       /* 내부 차지 펌프 켜기 (3.3V 단일 전원 모듈) */
        0x20, 0x00,       /* 수평 주소 지정 모드 */
        0xA1,             /* 세그먼트 리맵 (좌우 반전 보정) */
        0xC8,             /* COM 스캔 방향 반전 (상하 보정) */
        0xDA, 0x12,       /* COM 핀 하드웨어 구성 */
        0x81, 0xCF,       /* 대비 */
        0xD9, 0xF1,       /* 프리차지 기간 */
        0xDB, 0x40,       /* VCOMH 레벨 */
        0xA4,             /* RAM 내용을 표시 */
        0xA6,             /* 정상(비반전) 표시 */
        0xAF              /* 디스플레이 켜기 */
    };
    return oled_cmds(init, sizeof init);
}

static HAL_StatusTypeDef oled_flush(void)
{
    static const uint8_t win[] = { 0x21, 0, OLED_W - 1, 0x22, 0, 7 };       /* 열 0..127, 페이지 0..7 */
    if (oled_cmds(win, sizeof win) != HAL_OK) return HAL_ERROR;
    return HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, fb, sizeof fb, 200);   /* 제어 바이트 0x40 = 데이터 */
}

/* ---------------- 그리기 (프레임버퍼) ---------------- */
static void px(int x, int y, int on)
{
    if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) return;
    uint8_t *b = &fb[(y >> 3) * OLED_W + x];                              /* 페이지 = y/8, 비트 = y%8 (LSB 가 위) */
    if (on) *b |= (uint8_t)(1u << (y & 7)); else *b &= (uint8_t)~(1u << (y & 7));
}

static void fill_rect(int x, int y, int w, int h, int on)
{
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, on);
}

static void rect(int x, int y, int w, int h)
{
    for (int i = 0; i < w; i++) { px(x + i, y, 1); px(x + i, y + h - 1, 1); }
    for (int j = 0; j < h; j++) { px(x, y + j, 1); px(x + w - 1, y + j, 1); }
}

static void line(int x0, int y0, int x1, int y1)                          /* Bresenham */
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;          /* dy 는 음수로 유지 */
    int err = dx + dy;
    for (;;)
    {
        px(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* 3x5 글꼴: 한 글자 = 5행, 각 행 3비트 (bit2 = 왼쪽 픽셀) */
static const struct { char c; uint8_t row[5]; } font[] = {
    {'0',{7,5,5,5,7}}, {'1',{2,6,2,2,7}}, {'2',{7,1,7,4,7}}, {'3',{7,1,7,1,7}}, {'4',{5,5,7,1,1}},
    {'5',{7,4,7,1,7}}, {'6',{7,4,7,5,7}}, {'7',{7,1,2,2,2}}, {'8',{7,5,7,5,7}}, {'9',{7,5,7,1,7}},
    {':',{0,2,0,2,0}}, {'.',{0,0,0,0,2}}, {'-',{0,0,7,0,0}}, {' ',{0,0,0,0,0}},
    {'F',{7,4,6,4,4}}, {'P',{7,5,7,4,4}}, {'S',{7,4,7,1,7}},
};

static void text(int x, int y, int scale, const char *s)
{
    for (; *s; s++, x += 4 * scale)                                        /* 글자 폭 3 + 간격 1 */
    {
        for (unsigned k = 0; k < sizeof font / sizeof font[0]; k++)
        {
            if (font[k].c != *s) continue;
            for (int r = 0; r < 5; r++)
                for (int c = 0; c < 3; c++)
                    if ((font[k].row[r] >> (2 - c)) & 1) fill_rect(x + c * scale, y + r * scale, scale, scale, 1);
            break;
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    if (oled_init() != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\nOLED not found at 0x3C (check wiring)\r\n", 41, 100);
        Error_Handler();
    }

    int bx = 20, by = 20, vx = 2, vy = 1;                                  /* 공 위치/속도 */
    uint32_t frame = 0, fps = 0, frames_this_sec = 0, t_sec = HAL_GetTick();
    char buf[24];
    while (1)
    {
        memset(fb, 0, sizeof fb);
        rect(0, 0, OLED_W, OLED_H);                                        /* 화면 테두리 */
        line(0, 0, 127, 63);                                               /* 대각선 */
        line(0, 63, 127, 0);

        bx += vx; by += vy;                                                /* 공: 4x4, 테두리 안에서 반사 */
        if (bx < 2 || bx > OLED_W - 6) { vx = -vx; bx += vx; }
        if (by < 2 || by > OLED_H - 6) { vy = -vy; by += vy; }
        fill_rect(bx, by, 4, 4, 1);

        snprintf(buf, sizeof buf, "%lu", (unsigned long)frame);
        text(6, 6, 3, buf);                                                /* 프레임 번호 (크게) */
        snprintf(buf, sizeof buf, "FPS %lu", (unsigned long)fps);
        text(6, 46, 2, buf);

        if (oled_flush() != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)"flush error\r\n", 13, 100);
            HAL_Delay(100);
        }

        frame++; frames_this_sec++;
        if (HAL_GetTick() - t_sec >= 1000)
        {
            t_sec += 1000;
            fps = frames_this_sec; frames_this_sec = 0;
            char m[40];
            int n = snprintf(m, sizeof m, "FPS = %lu\r\n", (unsigned long)fps);
            HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 100);
        }
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
    hi2c1.Init.ClockSpeed = 400000;                   /* 고속 모드 400kHz (PCLK1 = 32MHz >= 4MHz 조건 만족) */
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

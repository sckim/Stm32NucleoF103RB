/**
 * 02_CRC_Unit_HAL_c  —  [계층: HAL]  하드웨어 CRC 계산 유닛 (소프트웨어 구현과 비교)
 *
 * STM32F1 CRC 유닛의 사양 (RM0008 4장 CRC calculation unit) — 고정이며 바꿀 수 없다
 *   다항식 0x04C11DB7 (CRC-32/Ethernet 과 동일), 초기값 0xFFFFFFFF, 입력/출력 비트 반전 없음, 최종 XOR 없음
 *   => 표준명 "CRC-32/MPEG-2".  입력은 32비트 워드 단위로 CRC->DR 에 쓰고, 결과는 CRC->DR 에서 읽는다 (1워드 4 AHB 사이클).
 *   * zlib/PNG/ZIP 이 쓰는 표준 CRC-32 는 입력·출력 비트 반전과 최종 XOR 이 있어 값이 다르다.
 *     (이후 STM32 시리즈의 CRC 는 다항식/반전 설정이 가능하지만 F1 은 불가능)
 *
 * 동작
 *   1) 256워드(1KB) 데이터의 CRC 를 HW(HAL_CRC_Calculate)와 SW(비트 단위 루프)로 계산해 결과 일치를 확인 (PASS/FAIL)
 *   2) 두 방식의 소요 시간을 DWT 사이클로 측정해 비교 (HW 가 수십 배 빠름)
 *   3) 데이터 1비트를 바꾸면 CRC 가 완전히 달라지는 것을 확인 (오류 검출 능력)
 *   활용: 펌웨어 무결성 검사(부트로더가 Flash 전체 CRC 확인), 통신 프레임 검사, 플래시 저장 데이터 유효성 표시.
 *
 * 하드웨어 대응
 *   CRC->CR.RESET(bit0)=1 : DR 을 초기값 0xFFFFFFFF 로,  CRC->DR : 쓰면 누적 계산, 읽으면 현재 CRC,  CRC->IDR : 범용 1바이트
 *   RCC->AHBENR.CRCEN(bit6) : 클럭 허용
 *
 * 인터럽트를 사용하지 않는다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define N_WORDS 256

CRC_HandleTypeDef hcrc;
UART_HandleTypeDef huart2;

static uint32_t data[N_WORDS];

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_CRC_Init(void);

/* CRC-32/MPEG-2 의 소프트웨어 구현: 워드를 XOR 한 뒤 MSB 부터 32번 시프트/다항식 XOR */
static uint32_t crc32_mpeg2_sw(const uint32_t *w, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < n; i++)
    {
        crc ^= w[i];
        for (int b = 0; b < 32; b++)
            crc = (crc & 0x80000000UL) ? (crc << 1) ^ 0x04C11DB7UL : (crc << 1);
    }
    return crc;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_CRC_Init();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (uint32_t i = 0; i < N_WORDS; i++) data[i] = 0x9E3779B9UL * (i + 1);   /* 임의 패턴 */

    char msg[128];

    uint32_t t0 = DWT->CYCCNT;
    uint32_t hw = HAL_CRC_Calculate(&hcrc, data, N_WORDS);      /* CR.RESET 후 DR 에 256워드 기록, DR 읽기 */
    uint32_t hw_cyc = DWT->CYCCNT - t0;

    t0 = DWT->CYCCNT;
    uint32_t sw = crc32_mpeg2_sw(data, N_WORDS);
    uint32_t sw_cyc = DWT->CYCCNT - t0;

    int n = snprintf(msg, sizeof msg, "\r\nHW CRC = 0x%08lX (%lu cycles)\r\nSW CRC = 0x%08lX (%lu cycles)  -> %s, HW is %lux faster\r\n",
                     (unsigned long)hw, (unsigned long)hw_cyc, (unsigned long)sw, (unsigned long)sw_cyc,
                     hw == sw ? "MATCH" : "MISMATCH", (unsigned long)(hw_cyc ? sw_cyc / hw_cyc : 0));
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);

    /* 오류 검출: 1비트만 뒤집어도 CRC 가 완전히 달라진다 */
    data[100] ^= 0x00000001UL;
    uint32_t hw2 = HAL_CRC_Calculate(&hcrc, data, N_WORDS);
    n = snprintf(msg, sizeof msg, "1-bit flip -> CRC = 0x%08lX (%s)\r\n", (unsigned long)hw2,
                 hw2 != hw ? "detected" : "NOT detected");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 200);

    while (1) {}
}

void HAL_CRC_MspInit(CRC_HandleTypeDef *hc)
{
    if (hc->Instance == CRC) __HAL_RCC_CRC_CLK_ENABLE();        /* RCC->AHBENR.CRCEN */
}

static void MX_CRC_Init(void)
{
    hcrc.Instance = CRC;
    if (HAL_CRC_Init(&hcrc) != HAL_OK) Error_Handler();
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

/**
 * 03_I2C_Slave_Loopback_HAL_c  —  [계층: HAL]  I2C 슬레이브 모드: 보드 한 장 안에서 I2C1(마스터) <-> I2C2(슬레이브) 시험
 *
 * 배선 (점퍼선 2개 + 풀업)
 *   PB6 (I2C1_SCL) ---- PB10 (I2C2_SCL)
 *   PB7 (I2C1_SDA) ---- PB11 (I2C2_SDA)
 *   SCL, SDA 각각 3.3V 로 4.7kΩ 풀업 권장 (내부 풀업만으로는 100kHz 에서 신호가 둔해질 수 있다)
 *
 * 역할
 *   I2C1 = 마스터,  I2C2 = 슬레이브 (7비트 주소 0x32). 슬레이브는 16바이트 "레지스터 파일"을 가진 가상 센서처럼 동작한다.
 *   전송 규약(모든 프레임 길이를 고정해 HAL 순차 API 로 단순하게 처리):
 *     쓰기  : 마스터가 2바이트 [reg, value] 송신          -> regs[reg] = value
 *     읽기  : 마스터가 2바이트 [0x80|reg, 0] 송신(포인터 설정) -> 이어서 4바이트 수신 = regs[reg .. reg+3]
 *
 * 이 예제가 보여주는 것
 *   - HAL_I2C_EnableListen_IT(): 슬레이브가 자기 주소 호출을 기다리는 상태(리슨)
 *   - 주소 일치 콜백(HAL_I2C_AddrCallback)에서 방향(TransferDirection)에 따라 수신/송신을 시작하는 슬레이브 순차 API
 *   - 전송이 끝나면 다시 리슨으로 복귀 (ListenCplt 콜백) — 슬레이브가 항상 응답 가능한 상태를 유지하는 구조
 *   결과: 마스터가 쓴 값을 읽어 되돌려 받아 일치하는지 검증하고 UART(115200)로 PASS/FAIL 출력.
 *
 * 하드웨어 대응
 *   I2C2->OAR1 : 자기 주소(0x32<<1),  CR1.ACK/ENGC,  SR1.ADDR(주소 일치)/RXNE/TXE/STOPF/AF,  CR2.ITEVTEN/ITERREN/ITBUFEN
 *   슬레이브는 클럭 스트레칭(SCL 을 잡고 대기)으로 처리 시간을 확보한다 (NoStretchMode = DISABLE)
 *
 * ISR vs 콜백
 *   I2C2_EV_IRQHandler / I2C2_ER_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_I2C_EV_IRQHandler / HAL_I2C_ER_IRQHandler
 *     -> HAL_I2C_AddrCallback / HAL_I2C_SlaveRxCpltCallback / HAL_I2C_SlaveTxCpltCallback / HAL_I2C_ListenCpltCallback / ErrorCallback (이 파일)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define SLAVE_ADDR (0x32 << 1)

I2C_HandleTypeDef hi2c1;      /* 마스터 */
I2C_HandleTypeDef hi2c2;      /* 슬레이브 */
UART_HandleTypeDef huart2;

static uint8_t regs[16];               /* 슬레이브의 가상 레지스터 파일 */
static uint8_t rx_frame[2];            /* 슬레이브가 받은 2바이트 [reg, value] */
static uint8_t ptr;                    /* 읽기 포인터 */
static volatile uint32_t slave_writes, slave_reads;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Master_Init(void);
static void MX_I2C2_Slave_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    MX_I2C1_Master_Init();
    MX_I2C2_Slave_Init();

    if (HAL_I2C_EnableListen_IT(&hi2c2) != HAL_OK) Error_Handler();    /* 슬레이브: 주소 호출 대기 시작 */

    char msg[112];
    uint32_t pass = 0, fail = 0;
    for (uint32_t round = 0; ; round++)
    {
        uint8_t reg = (uint8_t)(round % 12);                           /* reg .. reg+3 이 배열 안에 있도록 */
        uint8_t val = (uint8_t)(0xA0 + (round & 0x0F));

        /* 쓰기: [reg, val] */
        uint8_t wr[2] = { reg, val };
        HAL_StatusTypeDef s1 = HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDR, wr, 2, 50);

        /* 읽기: [0x80|reg, 0] 로 포인터 설정 후 4바이트 수신 */
        uint8_t setp[2] = { (uint8_t)(0x80 | reg), 0 };
        uint8_t got[4] = {0};
        HAL_StatusTypeDef s2 = HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDR, setp, 2, 50);
        HAL_StatusTypeDef s3 = HAL_I2C_Master_Receive(&hi2c1, SLAVE_ADDR, got, 4, 50);

        int ok = (s1 == HAL_OK && s2 == HAL_OK && s3 == HAL_OK && got[0] == val);
        if (ok) pass++; else fail++;

        int n = snprintf(msg, sizeof msg, "wrote regs[%u]=0x%02X, read back 0x%02X 0x%02X 0x%02X 0x%02X  %s  (slave writes=%lu reads=%lu, pass=%lu fail=%lu)\r\n",
                         reg, val, got[0], got[1], got[2], got[3], ok ? "PASS" : "FAIL",
                         (unsigned long)slave_writes, (unsigned long)slave_reads, (unsigned long)pass, (unsigned long)fail);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
        HAL_Delay(500);
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): I2C2 = 슬레이브                                          */
/* ------------------------------------------------------------------------- */
/* 마스터가 우리 주소를 불렀다. TransferDirection 은 "마스터 기준"이다:
   I2C_DIRECTION_TRANSMIT = 마스터가 쓴다(슬레이브는 수신), I2C_DIRECTION_RECEIVE = 마스터가 읽는다(슬레이브는 송신) */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    (void)AddrMatchCode;
    if (hi2c->Instance != I2C2) return;
    if (TransferDirection == I2C_DIRECTION_TRANSMIT)
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, rx_frame, 2, I2C_FIRST_AND_LAST_FRAME);
    else
    {
        HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &regs[ptr & 0x0F], 4, I2C_FIRST_AND_LAST_FRAME);
        slave_reads++;
    }
}

/* 슬레이브 수신 완료: 2바이트 [reg, value] 처리 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C2) return;
    if (rx_frame[0] & 0x80) ptr = rx_frame[0] & 0x7F;                 /* 읽기용 포인터 설정 */
    else { regs[rx_frame[0] & 0x0F] = rx_frame[1]; slave_writes++; }  /* 레지스터 쓰기 */
}

/* 주소 호출 한 번의 전송이 모두 끝나면(STOP 또는 NACK) 다시 리슨으로 복귀해야 다음 호출에 응답한다 */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) HAL_I2C_EnableListen_IT(hi2c);
}

/* 슬레이브 송신에서 마스터가 마지막 바이트에 NACK 하는 것은 정상 종료 (AF). 그 외 오류도 리슨 복귀로 복구 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2)
    {
        if (HAL_I2C_GetError(hi2c) != HAL_I2C_ERROR_AF) { /* 필요 시 여기서 오류 통계 증가 */ }
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7;  g.Mode = GPIO_MODE_AF_OD;  g.Pull = GPIO_PULLUP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else if (hi2c->Instance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();                          /* RCC->APB1ENR.I2C2EN */
        g.Pin = GPIO_PIN_10 | GPIO_PIN_11;  g.Mode = GPIO_MODE_AF_OD;  g.Pull = GPIO_PULLUP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &g);
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0, 0);  HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0, 0);  HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
}

static void MX_I2C1_Master_Init(void)
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

static void MX_I2C2_Slave_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 100000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = SLAVE_ADDR;                      /* OAR1 : 슬레이브 주소 */
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) Error_Handler();
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

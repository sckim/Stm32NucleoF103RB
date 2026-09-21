/**
 * 06_BKP_Tamper_HAL_c  —  [계층: HAL(UART/클럭) + 레지스터(BKP)]  백업 레지스터와 탬퍼(TAMPER) 감지
 *
 * 백업 레지스터(BKP_DRx)란 (RM0008 6장)
 *   VDD 가 꺼져도 VBAT(코인 셀)이 있으면 값이 유지되는 16비트 레지스터들(F103xB 는 DR1~DR10). 리셋/Standby 를 거쳐도 살아남는다.
 *   RTC 설정 유지 표시, 부팅 횟수, 설정값, 보안 키 저장 등에 쓴다. (VBAT 없이 VDD 만 있어도 리셋 간에는 유지되고, 완전 전원 차단 시 사라짐)
 *
 * 탬퍼(TAMPER) 감지 (RM0008 6.3.1)
 *   TAMPER 핀 = PC13. 핀에 지정된 레벨이 감지되면 하드웨어가 **모든 백업 레지스터를 즉시 0 으로 지우고** 이벤트/인터럽트를 낸다.
 *   케이스 개봉, 외부 침입 스위치가 열리면 키를 자동 삭제하는 보안 기능이다.
 *   BKP->CR.TPE(bit0) = 1 : 탬퍼 핀 활성화,  CR.TPAL(bit1) : 활성 레벨 (0 = High 에서 감지, 1 = Low 에서 감지)
 *   BKP->CSR.TEF(bit8) : 탬퍼 이벤트 플래그,  TIF(bit9) : 탬퍼 인터럽트 플래그,  TPIE(bit2) : 탬퍼 인터럽트 허용,  CTE(bit0)/CTI(bit1) : 플래그 삭제
 *   *** Nucleo 의 사용자 버튼 B1 이 바로 PC13 이다. 눌리면 Low -> TPAL=1 로 설정하면 "B1 누름 = 침입 발생"이 되어
 *       백업 레지스터가 지워지는 것을 손으로 확인할 수 있다. TPE=1 인 동안 PC13 은 일반 GPIO 로 쓸 수 없다. ***
 *
 * 동작
 *   부팅 시 백업 레지스터를 읽어 출력한다.
 *     - DR1 이 0xCAFE 이면 "이전 값이 살아 있음" (리셋 버튼을 눌러도 유지되는 것을 확인)
 *     - 아니면(첫 부팅 또는 탬퍼로 지워짐) DR1=0xCAFE, DR2=부팅 횟수 0, DR3=0x1234 로 초기화
 *   실행 중 매 부팅마다 DR2(부팅 횟수)를 1 증가.
 *   B1(PC13)을 누르면 탬퍼 인터럽트 -> DR1~DR10 이 지워진 것을 출력. 이어서 다음 리셋 때 "지워졌었음"이 확인된다.
 *
 * ISR vs 콜백
 *   TAMPER_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> tamper_isr() (이 파일): BKP->CSR.TIF 확인, 플래그 삭제
 *   (HAL 의 RTC 탬퍼 API 는 RTC 핸들이 필요해서, 이 예제는 BKP 레지스터를 직접 다룬다)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2;

static volatile uint8_t tamper_flag;

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);

/* ISR 에서 호출됨 (stm32f1xx_it.c 의 TAMPER_IRQHandler) */
void tamper_isr(void)
{
    if (BKP->CSR & BKP_CSR_TIF)                 /* 탬퍼 인터럽트 플래그 */
    {
        BKP->CSR |= BKP_CSR_CTI | BKP_CSR_CTE;  /* CTI, CTE = 1 : 플래그 삭제 */
        BKP->CSR &= ~BKP_CSR_TPIE;              /* 핀이 계속 Low 이면 인터럽트가 폭주하므로 잠시 끔 (main 이 다시 켬) */
        tamper_flag = 1;
    }
}

static void dump_bkp(const char *title)
{
    char m[128];
    int n = snprintf(m, sizeof m, "%s DR1=0x%04X DR2=%u DR3=0x%04X ... DR10=0x%04X\r\n", title,
                     (unsigned)BKP->DR1, (unsigned)BKP->DR2, (unsigned)BKP->DR3, (unsigned)BKP->DR10);
    HAL_UART_Transmit(&huart2, (uint8_t *)m, (uint16_t)n, 100);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    __HAL_RCC_PWR_CLK_ENABLE();                 /* RCC->APB1ENR.PWREN */
    __HAL_RCC_BKP_CLK_ENABLE();                 /* RCC->APB1ENR.BKPEN */
    HAL_PWR_EnableBkUpAccess();                 /* PWR->CR.DBP = 1 : 백업 도메인 쓰기 허용 */

    print("\r\n[BKP + tamper]\r\n");
    dump_bkp("at boot :");

    if (BKP->DR1 != 0xCAFE)
    {
        print("backup registers were empty/erased -> initializing\r\n");
        BKP->DR1 = 0xCAFE;
        BKP->DR2 = 0;
        BKP->DR3 = 0x1234;
    }
    BKP->DR2 = (uint16_t)(BKP->DR2 + 1);        /* 부팅 횟수 */
    dump_bkp("updated :");

    /* 탬퍼 설정: PC13, Low 에서 감지 (B1 누름), 인터럽트 허용 */
    BKP->CSR |= BKP_CSR_CTE | BKP_CSR_CTI;      /* 이전 플래그 삭제 */
    BKP->CR = BKP_CR_TPAL | BKP_CR_TPE;         /* TPAL=1 (Low 감지), TPE=1 (탬퍼 핀 활성) */
    BKP->CSR |= BKP_CSR_TPIE;                   /* 탬퍼 인터럽트 허용 */
    HAL_NVIC_SetPriority(TAMPER_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TAMPER_IRQn);
    print("tamper armed: press B1 (PC13 low) to erase backup registers; press NRST to see they survive reset\r\n");

    while (1)
    {
        if (tamper_flag)
        {
            tamper_flag = 0;
            print("*** TAMPER detected ***\r\n");
            dump_bkp("after   :");              /* 하드웨어가 이미 0 으로 지웠다 */
            HAL_Delay(1000);                    /* 버튼을 뗄 시간 */
            BKP->CSR |= BKP_CSR_CTE | BKP_CSR_CTI;
            BKP->CSR |= BKP_CSR_TPIE;           /* 탬퍼 인터럽트 재무장 */
        }
    }
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

/* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용 MSP */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
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

/**
 * 05_Standby_RTC_Wakeup_HAL_c  —  [계층: HAL]  Standby 모드 + RTC 알람 웨이크업 + 백업 레지스터 유지
 *
 * 동작
 *   부팅 -> (Standby 복귀인지 판별) -> BKP 부팅 카운터 +1 -> LD2 1초 점등 -> RTC 알람 = 지금 + 10초 설정
 *   -> Standby 진입.  10초 뒤 RTC 알람이 MCU 를 깨우면 "리셋과 동일하게" main 부터 다시 시작한다.
 *   UART(115200)에는 리셋 원인(POWER-ON / STANDBY-WAKEUP)과 BKP 부팅 카운터가 출력된다.
 *   WKUP 핀(PA0)에 3.3V 를 인가해도 깨어난다.
 *
 * Standby vs Stop vs Sleep (RM0008 5장)
 *   Sleep   : CPU 클럭만 정지, 모든 상태 유지, 웨이크업 즉시            (03_Sleep_Mode)
 *   Stop    : 고속 클럭 정지, SRAM/레지스터 유지, 웨이크업 후 이어서 실행  (04_Stop_Mode_EXTI)
 *   Standby : 1.8V 도메인 전원 차단. SRAM·레지스터 내용 "소실", 웨이크업 = 리셋.  가장 낮은 전류(약 2uA)
 *             유지되는 것: 백업 도메인(RTC, BKP 백업 레지스터 DR1~DR10) 과 Standby 회로
 *   -> Standby 에서 상태를 이어가려면 BKP 레지스터/백업 SRAM 에 저장해 두어야 한다.
 *
 * 핵심 레지스터/비트
 *   PWR->CR.PDDS=1 + SCB->SCR.SLEEPDEEP=1 + WFI  (HAL_PWR_EnterSTANDBYMode)
 *   PWR->CSR.SBF : "Standby 에서 깨어났음" 플래그 (CR.CSBF 에 1 을 써서 지움)
 *   PWR->CSR.EWUP: WKUP 핀(PA0) 웨이크업 허용
 *   RTC->ALRH/ALRL : 알람 값,  RTC->CRH.ALRIE : 알람 인터럽트 허용,  RTC->CRL.ALRF : 알람 플래그
 *   BKP->DR1~DR10 : 백업 레지스터 (PWR->CR.DBP = 1 로 쓰기 허용 필요). HAL_RTCEx_BKUPRead/Write
 *   RTC 클럭 소스: LSE(32.768kHz 크리스털)를 먼저 시도하고, 없으면 LSI(약 40kHz)로 대체
 *
 * 주의: Standby 중에는 SWD 디버거 연결이 끊긴다. 전류를 잴 때는 IDD 점퍼(JP6)로 측정.
 *       RTC 알람 웨이크업 후 HAL_RTC 를 다시 초기화하지만, 백업 도메인 리셋(BDRST)은 절대 하지 않는다 (BKP 유지).
 *
 * ISR: RTC 알람은 Standby 웨이크업(리셋) 용도라 별도 ISR/콜백이 필요 없다.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>

#define BKP_MAGIC_REG   RTC_BKP_DR1      /* RTC/백업 도메인이 이미 설정됐는지 표시 */
#define BKP_MAGIC       0x32F2U
#define BKP_COUNT_REG   RTC_BKP_DR2      /* 부팅 횟수 */
#define ALARM_SECONDS   10U

RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);


int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    __HAL_RCC_PWR_CLK_ENABLE();                       /* RCC->APB1ENR.PWREN */
    __HAL_RCC_BKP_CLK_ENABLE();                       /* RCC->APB1ENR.BKPEN */
    HAL_PWR_EnableBkUpAccess();                       /* PWR->CR.DBP = 1 */

    int from_standby = __HAL_PWR_GET_FLAG(PWR_FLAG_SB);   /* PWR->CSR.SBF */
    if (from_standby) __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  /* PWR->CR.CSBF = 1 */

    MX_RTC_Init();

    uint32_t cnt = HAL_RTCEx_BKUPRead(&hrtc, BKP_COUNT_REG) + 1;
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_COUNT_REG, cnt);

    char msg[96];
    int n = snprintf(msg, sizeof msg, "\r\n[boot #%lu] reset cause: %s\r\n", (unsigned long)cnt,
                     from_standby ? "STANDBY-WAKEUP (RTC alarm or WKUP pin)" : "POWER-ON / RESET");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    /* 알람 = 현재 RTC 시각 + 10초 (시:분:초 로 환산, 24시간 래핑 처리) */
    RTC_TimeTypeDef t;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    uint32_t sec = (t.Hours * 3600U + t.Minutes * 60U + t.Seconds + ALARM_SECONDS) % 86400U;
    RTC_AlarmTypeDef a = {0};
    a.Alarm = RTC_ALARM_A;                             /* F1 에는 알람이 하나 (필드는 시분초만 사용) */
    a.AlarmTime.Hours   = (uint8_t)(sec / 3600U);
    a.AlarmTime.Minutes = (uint8_t)((sec / 60U) % 60U);
    a.AlarmTime.Seconds = (uint8_t)(sec % 60U);
    if (HAL_RTC_SetAlarm_IT(&hrtc, &a, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();   /* RTC->ALR, CRH.ALRIE */

    n = snprintf(msg, sizeof msg, "alarm set for %02u:%02u:%02u, entering STANDBY...\r\n",
                 a.AlarmTime.Hours, a.AlarmTime.Minutes, a.AlarmTime.Seconds);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);

    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF); /* 진입 전 오래된 알람 플래그 제거 */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);                 /* PWR->CR.CWUF: 이전 웨이크업 플래그 제거 */
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);          /* PWR->CSR.EWUP = 1 : PA0 웨이크업 허용 */

    HAL_PWR_EnterSTANDBYMode();                        /* 여기서 정지. 깨어나면 main 처음부터 다시 시작 */
    while (1) {}                                       /* 도달하지 않음 */
}

/* HAL_RTC_Init 이 호출: RTC 클럭 소스가 아직 없을 때만(=전원 최초 인가) 선택한다.
   이미 선택돼 있으면 건드리지 않는다 -> 변경하려면 백업 도메인 리셋이 필요해 BKP/RTC 값이 지워지기 때문. */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc_)
{
    (void)hrtc_;
    if (__HAL_RCC_GET_RTC_SOURCE() == RCC_RTCCLKSOURCE_NO_CLK)   /* RCC->BDCR.RTCSEL == 00 */
    {
        RCC_OscInitTypeDef o = {0};
        RCC_PeriphCLKInitTypeDef pc = {0};

        o.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        o.LSEState = RCC_LSE_ON;                       /* RCC->BDCR.LSEON (시작에 최대 수 초) */
        o.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&o) == HAL_OK)
        {
            pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
        }
        else                                           /* LSE 크리스털이 없거나 시작 실패 -> LSI 로 대체 */
        {
            o.OscillatorType = RCC_OSCILLATORTYPE_LSI;
            o.LSEState = RCC_LSE_OFF;
            o.LSIState = RCC_LSI_ON;
            if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();
            pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
        }
        pc.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();   /* RCC->BDCR.RTCSEL */
    }
    __HAL_RCC_RTC_ENABLE();                            /* RCC->BDCR.RTCEN */
}

static void MX_RTC_Init(void)
{
    hrtc.Instance = RTC;
    hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;        /* 선택된 클럭 소스에서 1초 분주비를 자동 계산 (RTC->PRL) */
    hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;  /* LD2 */
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

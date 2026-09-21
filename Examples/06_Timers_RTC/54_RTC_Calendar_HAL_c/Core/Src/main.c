/**
 * 54_RTC_Calendar_HAL_c  —  [계층: HAL(RTC 초기화/1초 인터럽트) + 레지스터(카운터 접근)]  RTC 달력 시계
 *
 * STM32F1 RTC 의 특징
 *   F1 의 RTC 는 "달력" 하드웨어가 없다. 32비트 카운터(RTC->CNTH:CNTL)가 1초마다 1 증가할 뿐이다.
 *   -> 이 카운터를 Unix 시간(1970-01-01 00:00:00 부터 경과한 초)으로 쓰고, 날짜/시각은 소프트웨어로 계산한다.
 *   (이후 시리즈의 RTC 는 BCD 달력 하드웨어가 있지만 F1 은 없다. HAL 의 F1 달력 API 도 이 방식을 흉내 낸 것이다.)
 *
 * 동작
 *   - 전원 최초 인가 시 컴파일 시각(__DATE__ / __TIME__)으로 카운터를 설정하고 BKP DR1 에 "설정됨" 표시.
 *   - RTC 1초 인터럽트마다 카운터를 읽어 "YYYY-MM-DD HH:MM:SS (요일)" 을 UART(115200)로 출력, LD2 토글.
 *   - NRST(리셋) 버튼을 눌러도 시각이 이어진다 (백업 도메인이 유지되기 때문). 전원을 완전히 끄면 VBAT 이 없어 초기화된다.
 *   - B1(PC13) 을 누르면 컴파일 시각으로 다시 설정.
 *
 * 핵심 레지스터/비트
 *   RTC->CNTH:CNTL : 32비트 카운터.  읽기: CNTH-CNTL-CNTH 를 읽어 상위가 바뀌었으면 재시도(자리올림 경합 방지)
 *   쓰기 절차: RTC->CRL.RTOFF=1 대기 -> CRL.CNF=1(설정 모드) -> CNTH/CNTL 기록 -> CNF=0 -> RTOFF=1 대기
 *   RTC->CRH.SECIE : 1초 인터럽트 허용,  RTC->CRL.SECF : 1초 플래그
 *   PWR->CR.DBP=1 : 백업 도메인 쓰기 허용 (RTC/BKP 레지스터 접근에 필요)
 *   RCC->BDCR : LSEON/LSERDY, RTCSEL[9:8], RTCEN[15]
 *
 * ISR vs 콜백
 *   RTC_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_RTCEx_RTCIRQHandler()
 *     -> HAL_RTCEx_RTCEventCallback() (이 파일) : 1초 이벤트. 플래그만 세우고 출력은 main 에서.
 */
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BKP_MAGIC_REG RTC_BKP_DR1
#define BKP_MAGIC     0x32F2U

RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart2;

static volatile uint8_t second_flag;

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);

/* ---- RTC 32비트 카운터 접근 (레지스터) ---- */
static uint32_t rtc_get_epoch(void)
{
    uint16_t hi, lo;
    do {
        hi = (uint16_t)RTC->CNTH;
        lo = (uint16_t)RTC->CNTL;
    } while (hi != (uint16_t)RTC->CNTH);              /* 읽는 사이 자리올림이 있었으면 재시도 */
    return ((uint32_t)hi << 16) | lo;
}

static void rtc_set_epoch(uint32_t v)
{
    while (!(RTC->CRL & RTC_CRL_RTOFF)) {}            /* 이전 쓰기 완료 대기 */
    RTC->CRL |= RTC_CRL_CNF;                          /* 설정 모드 진입 */
    RTC->CNTH = v >> 16;
    RTC->CNTL = v & 0xFFFFU;
    RTC->CRL &= ~RTC_CRL_CNF;                         /* 설정 모드 탈출 (이때 실제 기록됨) */
    while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
}

/* ---- Unix 시간 <-> 날짜 변환 (그레고리력, 윤년 포함) ---- */
static void epoch_to_datetime(uint32_t e, int *y, int *mo, int *d, int *h, int *mi, int *s, int *wd)
{
    uint32_t days = e / 86400U, rem = e % 86400U;
    *h = (int)(rem / 3600U);  *mi = (int)((rem / 60U) % 60U);  *s = (int)(rem % 60U);
    *wd = (int)((days + 4U) % 7U);                    /* 1970-01-01 은 목요일(4). 0=일 */

    /* civil_from_days (H. Hinnant): 1970-01-01 기준 일수 -> 년/월/일 */
    int32_t z = (int32_t)days + 719468;
    int32_t era = z / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t yy = yoe + era * 400;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*mo <= 2));
}

static uint32_t datetime_to_epoch(int y, int mo, int d, int h, int mi, int s)
{
    y -= (mo <= 2);
    int32_t era = y / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int32_t days = era * 146097 + doe - 719468;
    return (uint32_t)days * 86400U + (uint32_t)(h * 3600 + mi * 60 + s);
}

/* __DATE__ = "Sep 21 2026", __TIME__ = "12:34:56" 를 파싱해 Unix 시간으로 */
static uint32_t compile_time_epoch(void)
{
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *dt = __DATE__;
    int mo = 1;
    for (int i = 0; i < 12; i++) if (strncmp(dt, &mon[i * 3], 3) == 0) { mo = i + 1; break; }
    int d = (dt[4] == ' ') ? (dt[5] - '0') : ((dt[4] - '0') * 10 + (dt[5] - '0'));
    int y = atoi(&dt[7]);
    const char *tm = __TIME__;
    int h = (tm[0] - '0') * 10 + (tm[1] - '0');
    int mi = (tm[3] - '0') * 10 + (tm[4] - '0');
    int s = (tm[6] - '0') * 10 + (tm[7] - '0');
    return datetime_to_epoch(y, mo, d, h, mi, s);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();                       /* PWR->CR.DBP */

    MX_RTC_Init();

    char msg[80];
    if (HAL_RTCEx_BKUPRead(&hrtc, BKP_MAGIC_REG) != BKP_MAGIC)     /* 최초 전원 인가 */
    {
        rtc_set_epoch(compile_time_epoch());
        HAL_RTCEx_BKUPWrite(&hrtc, BKP_MAGIC_REG, BKP_MAGIC);
        print("\r\nRTC set from compile time\r\n");
    }
    else
    {
        print("\r\nRTC already running (backup domain kept)\r\n");
    }

    HAL_RTCEx_SetSecond_IT(&hrtc);                     /* RTC->CRH.SECIE = 1 */

    static const char *const wdn[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    while (1)
    {
        if (second_flag)
        {
            second_flag = 0;
            int y, mo, d, h, mi, s, wd;
            epoch_to_datetime(rtc_get_epoch(), &y, &mo, &d, &h, &mi, &s, &wd);
            int n = snprintf(msg, sizeof msg, "%04d-%02d-%02d %02d:%02d:%02d (%s)\r\n", y, mo, d, h, mi, s, wdn[wd]);
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)   /* B1 : 컴파일 시각으로 재설정 */
        {
            HAL_Delay(30);
            rtc_set_epoch(compile_time_epoch());
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {}
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 아님): RTC 1초 이벤트 (SECF)                                    */
/* ------------------------------------------------------------------------- */
void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc_)
{
    (void)hrtc_;
    second_flag = 1;
}

/* 클럭 소스는 최초 전원 인가 때만 선택 (변경하면 백업 도메인 리셋으로 시각이 사라짐). LSE 실패 시 LSI 대체 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc_)
{
    (void)hrtc_;
    if (__HAL_RCC_GET_RTC_SOURCE() == RCC_RTCCLKSOURCE_NO_CLK)
    {
        RCC_OscInitTypeDef o = {0};
        RCC_PeriphCLKInitTypeDef pc = {0};

        o.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        o.LSEState = RCC_LSE_ON;
        o.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&o) == HAL_OK)
        {
            pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
        }
        else
        {
            o.OscillatorType = RCC_OSCILLATORTYPE_LSI;
            o.LSEState = RCC_LSE_OFF;
            o.LSIState = RCC_LSI_ON;
            if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();
            pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
        }
        pc.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();
    }
    __HAL_RCC_RTC_ENABLE();
    HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

static void MX_RTC_Init(void)
{
    hrtc.Instance = RTC;
    hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
    hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;  /* LD2 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_INPUT;      g.Pull = GPIO_NOPULL;                                /* B1 */
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

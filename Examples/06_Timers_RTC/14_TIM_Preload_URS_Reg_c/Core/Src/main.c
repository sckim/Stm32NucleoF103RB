/**
 * 14_TIM_Preload_URS_Reg_c  —  [계층: 레지스터(CMSIS)]  ARR 이중 버퍼(ARPE)와 UG/URS가 만드는 함정을 실측
 *
 * 교안 「타이머(TIM): 카운터와 비교기로 시간을 만드는 장치」 6.1절(시간 기반 유닛과 이중 버퍼)의 실습이다.
 *
 * 실험 A — URS와 "시작하자마자 들어오는 인터럽트"
 *   초기화 끝에 EGR.UG로 갱신 이벤트(UEV)를 강제로 일으키면 PSC·ARR 프리로드 값이 섀도 레지스터로 옮겨진다.
 *   CR1.URS = 0이면 이 UG도 SR.UIF를 세우므로, DIER.UIE가 이미 1이면 CEN을 켜기도 전에 인터럽트가 1번 들어온다.
 *   CR1.URS = 1이면 UG는 UIF를 세우지 않는다(카운터 오버플로만 UIF를 세운다).
 *   → CEN = 0 상태에서 UG만 쓰고 ISR 진입 횟수를 센다. 기대값: URS=0 → 1, URS=1 → 0.
 *
 * 실험 B — ARPE와 "긴 주기" 결함
 *   TIM3을 1 µs 카운트(PSC=63), ARR=999(1 ms 주기)로 돌리다가, 한 주기 중간(CNT ≥ 700)에서 ARR을 499로 줄인다.
 *   CR1.ARPE = 1이면 새 ARR은 다음 UEV에서 섀도로 옮겨지므로 주기가 1000 → 500 µs로 깔끔하게 바뀐다.
 *   CR1.ARPE = 0이면 새 ARR이 즉시 섀도에 반영된다. CNT는 이미 499를 지났으므로 CNT == ARR 일치를 만나지 못하고
 *   0xFFFF까지 올라가 0으로 넘어간 뒤에야 499에 닿는다 → 한 주기가 약 65~66 ms로 늘어난다.
 *   0xFFFF→0 랩 지점에서 UEV가 나는지(65.5 ms) 아니면 다음 499 일치에서야 나는지(66.0 ms)는 출력값으로 확인한다.
 *   → UEV마다 DWT->CYCCNT(64 MHz 사이클 카운터)로 시각을 찍어 주기를 µs로 출력한다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, AHB ÷1, APB1 ÷2(PCLK1 32 MHz) → APB1 타이머 클럭 ×2 = 64 MHz
 *
 * 사용 레지스터·비트 (RM0008 15.4절)
 *   TIM3->CR1 : CEN(bit0) 카운터 시작, URS(bit2) UIF 발생원 제한, ARPE(bit7) ARR 프리로드
 *   TIM3->DIER: UIE(bit0) 갱신 인터럽트 허용
 *   TIM3->SR  : UIF(bit0) 갱신 플래그 (rc_w0: 0을 써서 지운다)
 *   TIM3->EGR : UG(bit0) 소프트웨어 갱신 이벤트
 *   TIM3->PSC, TIM3->ARR, TIM3->CNT
 *   DWT->CYCCNT, CoreDebug->DEMCR.TRCENA : 사이클 단위 시각 측정 (08_Clock_System/10_DWT_Profiling_Reg_c 참고)
 *
 * ISR: TIM3_IRQHandler (벡터 테이블의 weak 심볼 재정의). HAL 콜백은 쓰지 않는다.
 *      ISR은 UIF를 지우고, 진입 횟수와 시각만 기록한다. 출력은 모두 main 루프가 한다.
 *
 * 출력: USART2(PA2, ST-LINK 가상 COM) 115200 8N1. 배선 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define N_STAMP     10u

static volatile uint32_t g_irq_count;          /* TIM3 ISR 진입 횟수 */
static volatile uint32_t g_stamp[N_STAMP];     /* UEV 시각(DWT 사이클) */
static volatile uint32_t g_nstamp;             /* 기록된 시각 수. N_STAMP 이상이면 기록하지 않는다 */

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    /* 48 MHz < SYSCLK ≤ 72 MHz → Flash 2 웨이트 스테이트를 먼저 설정 (RM0008 3.3.3, FLASH_ACR.LATENCY = 010) */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    /* RCC_CFGR: PLLSRC = 0(HSI/2), PLLMULL = ×16, HPRE = ÷1, PPRE1 = ÷2, PPRE2 = ÷1 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;
    RCC->CR |= RCC_CR_PLLON;                                   /* PLL 켜기 */
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}                      /* PLL 잠김 대기 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;  /* SYSCLK ← PLL */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClockUpdate();
}

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;            /* DWT 블록 전원 */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;                       /* 사이클 카운터 시작 */
}

static void delay_us(uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < us * CYC_PER_US) {}
}

static void UART2_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    /* PA2 = USART2_TX: 대체기능 푸시풀 50 MHz, CRL[11:8] = 0xB. PA3(RX)는 리셋값(입력 플로팅) 그대로 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);
    USART2->BRR = (PCLK1_HZ + 115200u / 2u) / 115200u;        /* 32 MHz / 115200 ≈ 278 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;                 /* 송신만 사용 */
}

static void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) {}
    USART2->DR = (uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_putu(uint32_t v)
{
    char buf[11];
    int i = 0;
    do { buf[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) uart_putc(buf[--i]);
}

/* ---------------- ISR ---------------- */

/* 진짜 ISR. UIF만 처리한다. 0을 써서 지우는 rc_w0 비트이므로 SR = ~UIF로 다른 플래그는 건드리지 않는다. */
void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF)
    {
        TIM3->SR = ~TIM_SR_UIF;
        g_irq_count++;
        if (g_nstamp < N_STAMP) g_stamp[g_nstamp++] = DWT->CYCCNT;
    }
}

/* ---------------- 실험 A: URS ---------------- */

static uint32_t Test_URS(uint32_t urs)
{
    TIM3->CR1  = 0;                         /* CEN = 0: 카운터는 끝까지 멈춰 있다 */
    TIM3->DIER = 0;
    TIM3->SR   = 0;
    TIM3->PSC  = 64 - 1;
    TIM3->ARR  = 1000 - 1;
    if (urs) TIM3->CR1 |= TIM_CR1_URS;      /* URS = 1: 오버플로만 UIF를 세운다 */

    g_nstamp    = N_STAMP;                  /* 이 실험에서는 시각을 기록하지 않는다 */
    g_irq_count = 0;
    TIM3->DIER  = TIM_DIER_UIE;             /* 인터럽트를 "먼저" 허용해 둔 흔한 실수 상황 */
    TIM3->EGR   = TIM_EGR_UG;               /* UG: 프리로드 → 섀도 복사, CNT·분주 카운터 0 */
    delay_us(100);                          /* 인터럽트가 들어올 시간 */

    uint32_t n = g_irq_count;
    TIM3->DIER = 0;
    TIM3->SR   = 0;
    return n;
}

/* ---------------- 실험 B: ARPE ---------------- */

static void Test_ARPE(uint32_t arpe)
{
    TIM3->CR1  = 0;
    TIM3->DIER = 0;
    TIM3->SR   = 0;
    TIM3->PSC  = 64 - 1;                    /* 64 MHz / 64 = 1 MHz → 1 카운트 = 1 µs */
    TIM3->ARR  = 1000 - 1;                  /* 1 ms 주기 */
    TIM3->CNT  = 0;
    TIM3->CR1  = TIM_CR1_URS | (arpe ? TIM_CR1_ARPE : 0u);
    TIM3->EGR  = TIM_EGR_UG;                /* 섀도 레지스터 채우기. URS=1이라 UIF는 서지 않는다 */

    g_nstamp   = 0;
    TIM3->DIER = TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;

    while (g_nstamp < 3u) {}                /* 정상 주기 2개 이상 기록 */
    while (TIM3->CNT < 700u) {}             /* 현재 주기의 중간, 새 ARR(499)을 이미 지난 시점 */
    TIM3->ARR = 500 - 1;                    /* ARPE=0: 즉시 섀도 반영 / ARPE=1: 다음 UEV에 반영 */

    uint32_t t0 = DWT->CYCCNT;
    while (g_nstamp < N_STAMP && (DWT->CYCCNT - t0) < 300000u * CYC_PER_US) {}

    TIM3->CR1  &= ~TIM_CR1_CEN;
    TIM3->DIER  = 0;

    uart_puts("  ARPE=");
    uart_putu(arpe);
    uart_puts(" : UEV period [us] =");
    for (uint32_t i = 1; i < g_nstamp; i++)
    {
        uart_putc(' ');
        uart_putu((g_stamp[i] - g_stamp[i - 1] + CYC_PER_US / 2u) / CYC_PER_US);
    }
    uart_puts("\r\n");
}

/* ---------------- main ---------------- */

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    UART2_Init();

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;     /* TIM3 클럭 (APB1 bit1) */
    /* PA5 = LD2: 출력 푸시풀 2 MHz, CRL[23:20] = 0x2 */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);

    uart_puts("\r\n14_TIM_Preload_URS_Reg_c  (SYSCLK 64 MHz, TIM3 clock 64 MHz, 1 count = 1 us)\r\n");

    for (uint32_t round = 1;; round++)
    {
        uart_puts("[round ");
        uart_putu(round);
        uart_puts("]\r\n");

        /* 실험 A */
        uart_puts("  A) UG with UIE=1, CEN=0 : ISR entries  URS=0 -> ");
        uart_putu(Test_URS(0));
        uart_puts(" , URS=1 -> ");
        uart_putu(Test_URS(1));
        uart_puts("   (expect 1 , 0)\r\n");

        /* 실험 B */
        uart_puts("  B) ARR 999 -> 499 written at CNT>=700\r\n");
        Test_ARPE(0);                       /* 기대: 1000 1000 (약 65500~66000) 500 500 ... */
        Test_ARPE(1);                       /* 기대: 1000 1000 1000 500 500 ... */

        GPIOA->ODR ^= (1u << 5);            /* LD2: 라운드마다 토글 */
        delay_us(3000000u);
    }
}

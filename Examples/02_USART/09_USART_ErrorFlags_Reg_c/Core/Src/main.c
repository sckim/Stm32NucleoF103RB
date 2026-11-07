/**
 * 09_USART_ErrorFlags_Reg_c  —  [계층: 레지스터(CMSIS)]  USART 상태 플래그(TXE·TC·RXNE)와 수신 오류(FE·NE·ORE)를 실측
 *
 * 교안 「직렬 통신(USART·SPI·I2C)」 USART 절(플래그의 의미, 보레이트 오차와 수신 허용 오차, 오버런)의 실습이다.
 *
 * 배선: PA9(USART1_TX, Arduino D8) ── 점퍼 ── PB11(USART3_RX, Morpho CN10-18)
 *       결과는 USART2(PA2, ST-LINK 가상 COM) 115200 8N1로 출력한다.
 *
 * 실험 A — TXE와 TC의 차이 (RM0008 27.3.2, 그림 282)
 *   TXE = "TDR이 비었다(다음 바이트를 써도 된다)", TC = "시프트 레지스터까지 비었다(선 위의 마지막 비트까지 끝났다)".
 *   USART1에 2바이트를 연달아 쓰고 각 플래그가 서는 시각을 DWT로 잰다.
 *   기대: 1번째 TXE ≈ 0 µs(시프트 레지스터가 비어 있어 즉시 옮겨짐), 2번째 TXE ≈ 1프레임(86.8 µs),
 *         TC ≈ 2프레임(173.6 µs). 1프레임 = 10비트 / 115200 bps.
 *
 * 실험 B — 보레이트 오차와 수신 허용 오차 (27.3.5, 표 193·194)
 *   수신기 USART3는 115200 bps(BRR = 278 = 0x116, USARTDIV = 17.375, 소수부 ≠ 0)로 고정한다.
 *   송신기 USART1의 보레이트를 115200 × (1 + e/1000)로 바꿔 가며 0x55·0xF0 패턴 200바이트를 보낸다.
 *   바이트마다 SR → DR 순서로 읽어(FE·NE·ORE 해제) 정상 수, 데이터 불일치, FE, NE, 누락을 센다.
 *   표 194에 따르면 M=0, 소수부 ≠ 0인 수신기의 허용 오차는 3.33 %(NF를 오류로 볼 때) ~ 3.88 %(NF 무시)이다.
 *   송신기 자신의 BRR 양자화 오차도 실제 오차에 더해지므로 출력의 "actual" 값을 함께 본다.
 *
 * 실험 C — 오버런 (27.3.3, USART_SR.ORE)
 *   수신기를 읽지 않은 채 5바이트를 받는다. RXNE가 1인 동안 다음 바이트가 끝나면 ORE가 서고,
 *   RDR(DR)에는 "첫 바이트"가 그대로 남으며 그 뒤의 바이트는 시프트 레지스터에서 사라진다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2(PCLK1 32 MHz: USART2·3), APB2 ÷1(PCLK2 64 MHz: USART1)
 *
 * 사용 레지스터·비트 (RM0008 27.6절)
 *   USART_SR : TXE(bit7), TC(bit6), RXNE(bit5), ORE(bit3), NE(bit2), FE(bit1)
 *              FE·NE·ORE는 "SR 읽기 → DR 읽기" 순서로 지워진다. TC는 "SR 읽기 → DR 쓰기"로 지워진다.
 *   USART_DR : 쓰면 TDR, 읽으면 RDR
 *   USART_BRR: 16 × USARTDIV = 반올림(fCK / baud). 하위 4비트가 소수부, 상위 12비트가 정수부이다.
 *   USART_CR1: UE(bit13), TE(bit3), RE(bit2)
 *   DWT->CYCCNT : 사이클 단위 시각 (08_Clock_System/10_DWT_Profiling_Reg_c 참고)
 *
 * 인터럽트를 사용하지 않는다(모두 폴링). HAL 콜백 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define PCLK2_HZ    64000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define BAUD        115200u
#define N_BYTES     200u

/* ---------------- 공통: 클럭, DWT ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트 스테이트 (LATENCY = 010) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;     /* HSI/2 × 16, APB1 ÷2, APB2 ÷1 */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClockUpdate();
}

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < us * CYC_PER_US) {}
}

/* BRR = 16 × USARTDIV = fCK / baud 를 반올림. 소수부 자리올림(예: 50.99 → 51.0)도 자동으로 처리된다. */
static uint32_t brr_of(uint32_t fck, uint32_t baud) { return (fck + baud / 2u) / baud; }

/* ---------------- USART2: 결과 출력 ---------------- */

static void Log_Init(void)
{
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 50 MHz */
    USART2->BRR = brr_of(PCLK1_HZ, BAUD);                     /* 278 = 0x116 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}
static void putc2(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void puts2(const char *s) { while (*s) putc2(*s++); }
static void putu2(uint32_t v)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) putc2(b[--i]);
}
static void putu2w(uint32_t v, int w)
{
    uint32_t t = v; int d = 1;
    while (t >= 10u) { t /= 10u; d++; }
    while (d++ < w) putc2(' ');
    putu2(v);
}
static void puthex2(uint32_t v, int digits)
{
    static const char h[] = "0123456789ABCDEF";
    puts2("0x");
    for (int i = digits - 1; i >= 0; i--) putc2(h[(v >> (4 * i)) & 0xFu]);
}
/* 부호 있는 정수를 소수점 둘째 자리까지 (v100 = 값 × 100) */
static void putfix2(int32_t v100)
{
    if (v100 < 0) { putc2('-'); v100 = -v100; } else putc2('+');
    putu2((uint32_t)v100 / 100u); putc2('.');
    putc2((char)('0' + ((uint32_t)v100 / 10u) % 10u)); putc2((char)('0' + (uint32_t)v100 % 10u));
}

/* ---------------- USART1 (송신) · USART3 (수신) ---------------- */

static void Link_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    /* PA9 = USART1_TX: AF 푸시풀 50 MHz (CRH[7:4] = 0xB) */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 4)) | (0xBu << 4);
    /* PB11 = USART3_RX: 입력 풀업 (CRH[15:12] = 1000, ODR11 = 1). 점퍼가 빠져도 선이 유휴(High)로 보인다 */
    GPIOB->CRH = (GPIOB->CRH & ~(0xFu << 12)) | (0x8u << 12);
    GPIOB->ODR |= (1u << 11);

    USART3->BRR = brr_of(PCLK1_HZ, BAUD);                     /* 수신기: 115200 고정 */
    USART3->CR1 = USART_CR1_UE | USART_CR1_RE;
}

/* 송신기 보레이트 변경: 진행 중인 프레임이 끝난 뒤(TC) UE를 끄고 BRR을 바꾼다 (27.3.4 Note) */
static void Tx_SetBRR(uint32_t brr)
{
    if (USART1->CR1 & USART_CR1_UE) { while (!(USART1->SR & USART_SR_TC)) {} }
    USART1->CR1 = 0;
    USART1->BRR = brr;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;                /* TE를 켜면 유휴 프레임(10비트 High)이 먼저 나간다 */
    while (!(USART1->SR & USART_SR_TC)) {}                    /* 유휴 프레임이 끝날 때까지 */
}

static void Rx_Flush(void)
{
    (void)USART3->SR; (void)USART3->DR;                       /* SR → DR 읽기: RXNE·FE·NE·ORE 해제 */
}

/* ---------------- 실험 A: TXE vs TC ---------------- */

static void Test_TXE_TC(void)
{
    Tx_SetBRR(brr_of(PCLK2_HZ, BAUD));
    (void)USART1->SR;                                         /* TC 해제 순서의 1단계: SR 읽기 */

    uint32_t t0 = DWT->CYCCNT;
    USART1->DR = 0x55;                                        /* TC 해제 2단계(DR 쓰기). 시프트 레지스터가 비어 있어 바로 옮겨진다 */
    while (!(USART1->SR & USART_SR_TXE)) {}
    uint32_t t_txe1 = DWT->CYCCNT;
    USART1->DR = 0xAA;                                        /* 두 번째 바이트는 TDR에서 기다린다 → TXE = 0 */
    while (!(USART1->SR & USART_SR_TXE)) {}
    uint32_t t_txe2 = DWT->CYCCNT;
    while (!(USART1->SR & USART_SR_TC)) {}
    uint32_t t_tc = DWT->CYCCNT;
    delay_us(300);
    Rx_Flush();

    puts2("A) USART1 115200, write 0x55 then 0xAA (1 frame = 86.8 us)\r\n");
    puts2("   TXE #1 after "); putu2((t_txe1 - t0) / CYC_PER_US);
    puts2(" us | TXE #2 after "); putu2((t_txe2 - t0) / CYC_PER_US);
    puts2(" us | TC after ");    putu2((t_tc - t0) / CYC_PER_US);
    puts2(" us   (expect ~0 / ~87 / ~174)\r\n");
}

/* ---------------- 실험 B: 보레이트 오차 ---------------- */

static const int16_t kErrPermille[] = { 0, 20, 30, 35, 40, 50, -20, -35, -40, -50 };

static void Test_BaudError(void)
{
    puts2("B) RX USART3 fixed 115200 (BRR 0x116). TX USART1 baud x (1+e), 200 bytes of 0x55/0xF0\r\n");
    puts2("   e(%)   TX BRR  actual(%)   ok  wrong  FE  NE  missing\r\n");
    for (uint32_t k = 0; k < sizeof kErrPermille / sizeof kErrPermille[0]; k++)
    {
        int32_t e = kErrPermille[k];                          /* 천분율 */
        uint32_t baud = (uint32_t)((int32_t)BAUD + ((int32_t)BAUD * e) / 1000);
        uint32_t brr = brr_of(PCLK2_HZ, baud);
        /* 실제 송신 보레이트 = PCLK2 / BRR. 115200 대비 오차를 0.01 % 단위로 */
        int32_t actual100 = (int32_t)(((int64_t)PCLK2_HZ * 10000LL) / ((int64_t)brr * BAUD) - 10000);

        Tx_SetBRR(brr);
        delay_us(200);
        Rx_Flush();

        uint32_t ok = 0, wrong = 0, fe = 0, ne = 0, got = 0;
        uint32_t sent = 0;
        uint32_t t_last = DWT->CYCCNT;
        while (sent < N_BYTES || (DWT->CYCCNT - t_last) < 500u * CYC_PER_US)
        {
            if (sent < N_BYTES && (USART1->SR & USART_SR_TXE))
            {
                USART1->DR = (sent & 1u) ? 0xF0 : 0x55;
                sent++;
                t_last = DWT->CYCCNT;
            }
            uint32_t sr = USART3->SR;                         /* 1단계: SR 읽기 (오류 플래그 보존) */
            if (sr & USART_SR_RXNE)
            {
                uint8_t d = (uint8_t)USART3->DR;              /* 2단계: DR 읽기 → RXNE·FE·NE·ORE 해제 */
                uint8_t expect = (got & 1u) ? 0xF0 : 0x55;
                if (sr & USART_SR_FE) fe++;
                if (sr & USART_SR_NE) ne++;
                if (!(sr & (USART_SR_FE | USART_SR_NE)) && d == expect) ok++;
                else if (d != expect) wrong++;
                got++;
            }
        }
        putc2(' '); putc2(' '); putc2(' ');
        putfix2(e * 10);                                      /* e(%) ×100 */
        puts2("  "); puthex2(brr, 3);
        puts2("   "); putfix2(actual100);
        puts2("  "); putu2w(ok, 4); puts2("  "); putu2w(wrong, 4);
        puts2(" "); putu2w(fe, 3); puts2(" "); putu2w(ne, 3);
        puts2("  "); putu2w(got >= N_BYTES ? 0 : N_BYTES - got, 4);
        puts2("\r\n");
    }
    puts2("   (RM0008 table 194: M=0, fraction != 0 -> tolerance 3.33 % with NE as error, 3.88 % ignoring NE)\r\n");
}

/* ---------------- 실험 C: 오버런 ---------------- */

static void Test_Overrun(void)
{
    static const uint8_t pkt[5] = { 0x31, 0x32, 0x33, 0x34, 0x35 };   /* '1'..'5' */
    Tx_SetBRR(brr_of(PCLK2_HZ, BAUD));
    delay_us(200);
    Rx_Flush();

    for (uint32_t i = 0; i < 5; i++)                          /* 수신기는 읽지 않는다 */
    {
        while (!(USART1->SR & USART_SR_TXE)) {}
        USART1->DR = pkt[i];
    }
    while (!(USART1->SR & USART_SR_TC)) {}
    delay_us(200);

    uint32_t sr = USART3->SR;
    uint8_t d = (uint8_t)USART3->DR;                          /* SR → DR: ORE 해제 */
    uint32_t sr_after = USART3->SR;
    puts2("C) sent '1'..'5' without reading USART3\r\n");
    puts2("   SR before read: RXNE="); putu2((sr >> 5) & 1u);
    puts2(" ORE="); putu2((sr >> 3) & 1u);
    puts2(" | DR = '"); putc2((char)d);
    puts2("' | after SR->DR read: RXNE="); putu2((sr_after >> 5) & 1u);
    puts2(" ORE="); putu2((sr_after >> 3) & 1u);
    puts2("   (expect ORE=1, DR='1', bytes '2'..'5' lost)\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    Log_Init();
    Link_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20); /* PA5 = LD2 */

    puts2("\r\n09_USART_ErrorFlags_Reg_c  (wire PA9/D8 -> PB11)\r\n");
    for (uint32_t round = 1;; round++)
    {
        puts2("[round "); putu2(round); puts2("]\r\n");
        Test_TXE_TC();
        Test_BaudError();
        Test_Overrun();
        GPIOA->ODR ^= (1u << 5);
        delay_us(5000000u);
    }
}

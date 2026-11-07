/**
 * 07_SPI_Modes_BSY_Reg_c  —  [계층: 레지스터(CMSIS)]  SPI 모드(CPOL/CPHA) 불일치 실험과 TXE·RXNE·BSY 순서 실측
 *
 * 교안 「직렬 통신(USART·SPI·I2C)」 SPI 절(4가지 모드, 전이중 "보내야 받는다", 플래그 순서)의 실습이다.
 *
 * 배선 (12_Advanced_Topics/04_SPI_Slave_Loopback_c와 같은 핀, NSS 선은 쓰지 않는다)
 *   PA5 (SPI1_SCK,  D13) ── PB13 (SPI2_SCK)
 *   PA7 (SPI1_MOSI, D11) ── PB15 (SPI2_MOSI)
 *   PB14 (SPI2_MISO)     ── PA6  (SPI1_MISO, D12)
 *   PA5는 LD2와 같은 핀이라 전송 중 LED가 흐릿하게 켜진다(정상). 결과는 USART2(VCP) 115200 8N1.
 *
 * 실험 A — 모드 일치·불일치 (RM0008 25.3.1, 그림 240)
 *   슬레이브 SPI2는 모드 0(CPOL=0, CPHA=0)으로 고정한다. 마스터 SPI1의 모드를 0~3으로 바꾸며
 *   마스터가 보낸 바이트를 슬레이브가 무엇으로 받았는지, 슬레이브가 미리 장전한 바이트를 마스터가 무엇으로 받았는지 비교한다.
 *   기대: 모드 0만 양쪽이 모두 맞고, 나머지는 1비트 밀림이나 전혀 다른 값이 나온다. 어떤 값이 나오는지는 실측으로 확인한다.
 *   모드마다 두 SPI를 RCC 리셋 레지스터로 초기화해 앞 실험의 비트 카운터 어긋남이 다음 실험에 남지 않게 한다.
 *
 * 실험 B — 전이중 1바이트의 플래그 순서 (25.3.5, 그림 241)
 *   마스터가 DR에 쓴 순간을 0으로 두고 TXE가 다시 서는 시각, RXNE가 서는 시각, BSY가 내려가는 시각을 DWT로 잰다.
 *   SCK = 64 MHz / 256 = 250 kHz → 8비트 = 32 µs.
 *   기대: TXE는 거의 즉시(데이터가 시프트 레지스터로 옮겨짐), RXNE는 약 32 µs(8번째 비트 수신 뒤), BSY는 그 무렵 내려간다.
 *   "보내야 받는다": 마스터는 수신만 하고 싶어도 DR에 더미 바이트를 써야 클럭이 나간다.
 *
 * 분주: SPI1은 APB2(64 MHz). BR=000(÷2)은 32 MHz로 데이터시트 최대 18 Mbit/s를 넘으므로 쓰면 안 된다(÷4 = 16 MHz가 상한).
 *       이 예제는 슬레이브가 폴링으로 따라올 수 있도록 ÷256(250 kHz)을 쓴다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, APB1 ÷2(SPI2 32 MHz), APB2 ÷1(SPI1 64 MHz)
 *
 * 사용 레지스터·비트 (RM0008 25.5절)
 *   SPI_CR1: CPHA(bit0), CPOL(bit1), MSTR(bit2), BR[5:3], SPE(bit6), SSI(bit8), SSM(bit9)
 *            CPOL/CPHA는 SPE = 0일 때만 바꾼다(25.3.1 Note).
 *            마스터: SSM=1, SSI=1 (NSS를 내부 High로 두어 모드 고장 MODF 방지)
 *            슬레이브: SSM=1, SSI=0 (항상 선택된 상태)
 *   SPI_SR : RXNE(bit0), TXE(bit1), MODF(bit5), OVR(bit6), BSY(bit7)
 *   SPI_DR : 쓰면 송신 버퍼, 읽으면 수신 버퍼
 *   RCC_APB2RSTR.SPI1RST(bit12), RCC_APB1RSTR.SPI2RST(bit14)
 *
 * 인터럽트를 사용하지 않는다(폴링). HAL 콜백 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)
#define BR_DIV256   (7u << 3)                                  /* CR1.BR = 111: fPCLK / 256 */

static const uint8_t kTx[4] = { 0x96, 0x3C, 0xA5, 0x0F };     /* 마스터가 보내는 값 */
static const uint8_t kSl[4] = { 0x5A, 0xC3, 0x81, 0xF0 };     /* 슬레이브가 장전해 두는 값 */

/* ---------------- 공통 ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트 스테이트 (LATENCY = 010) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;
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

static void Log_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX */
    USART2->BRR = (PCLK1_HZ + 115200u / 2u) / 115200u;
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
static void putbin8(uint8_t v) { for (int i = 7; i >= 0; i--) putc2((v >> i) & 1u ? '1' : '0'); }

/* ---------------- 핀과 SPI 설정 ---------------- */

static void Pins_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    /* SPI1 마스터: PA5 SCK·PA7 MOSI = AF 푸시풀 50 MHz(0xB), PA6 MISO = 입력 플로팅(0x4) */
    GPIOA->CRL = (GPIOA->CRL & ~((0xFu << 20) | (0xFu << 24) | (0xFu << 28)))
               | (0xBu << 20) | (0x4u << 24) | (0xBu << 28);
    /* SPI2 슬레이브: PB13 SCK·PB15 MOSI = 입력 플로팅(0x4), PB14 MISO = AF 푸시풀 50 MHz(0xB) */
    GPIOB->CRH = (GPIOB->CRH & ~((0xFu << 20) | (0xFu << 24) | (0xFu << 28)))
               | (0x4u << 20) | (0xBu << 24) | (0x4u << 28);
}

/* 두 SPI를 리셋한 뒤 마스터 모드를 m(0~3), 슬레이브는 모드 0으로 설정 */
static void SPI_Setup(uint32_t m)
{
    RCC->APB2RSTR |=  RCC_APB2RSTR_SPI1RST;  RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    RCC->APB1RSTR |=  RCC_APB1RSTR_SPI2RST;  RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;

    /* 슬레이브 먼저 켠다: MSTR=0, 모드 0, SSM=1·SSI=0(항상 선택) */
    SPI2->CR1 = SPI_CR1_SSM;
    SPI2->CR1 |= SPI_CR1_SPE;

    /* 마스터: MSTR=1, BR=÷256, SSM=1·SSI=1, CPOL = m bit1, CPHA = m bit0 */
    SPI1->CR1 = SPI_CR1_MSTR | BR_DIV256 | SPI_CR1_SSM | SPI_CR1_SSI
              | ((m & 2u) ? SPI_CR1_CPOL : 0u) | ((m & 1u) ? SPI_CR1_CPHA : 0u);
    SPI1->CR1 |= SPI_CR1_SPE;                                   /* SPE를 켜는 순간 SCK가 CPOL 유휴 레벨로 간다 */
    delay_us(50);
}

/* 1바이트 교환: 슬레이브가 sl을 장전한 상태에서 마스터가 tx를 보낸다 */
static void Exchange(uint8_t tx, uint8_t sl, uint8_t *m_rx, uint8_t *s_rx)
{
    while (!(SPI2->SR & SPI_SR_TXE)) {}
    SPI2->DR = sl;                                              /* 클럭이 오기 전에 장전 (슬레이브는 클럭을 만들 수 없다) */
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    SPI1->DR = tx;                                              /* 이 쓰기가 SCK 8펄스를 만든다 */
    uint32_t t0 = DWT->CYCCNT;
    while (!(SPI1->SR & SPI_SR_RXNE)) { if ((DWT->CYCCNT - t0) > 200u * CYC_PER_US) break; }
    *m_rx = (uint8_t)SPI1->DR;
    t0 = DWT->CYCCNT;
    while (!(SPI2->SR & SPI_SR_RXNE)) { if ((DWT->CYCCNT - t0) > 200u * CYC_PER_US) break; }
    *s_rx = (uint8_t)SPI2->DR;
    while (SPI1->SR & SPI_SR_BSY) {}
}

/* ---------------- 실험 A ---------------- */

static void Test_Modes(void)
{
    puts2("A) slave SPI2 = mode 0 fixed, master SPI1 mode 0..3, SCK 250 kHz\r\n");
    for (uint32_t m = 0; m < 4; m++)
    {
        SPI_Setup(m);
        uint32_t ok_s = 0, ok_m = 0;
        puts2("   master mode "); putu2(m);
        puts2(" (CPOL="); putu2(m >> 1); puts2(",CPHA="); putu2(m & 1u); puts2(")\r\n");
        for (uint32_t i = 0; i < 4; i++)
        {
            uint8_t mr, sr;
            Exchange(kTx[i], kSl[i], &mr, &sr);
            ok_s += (sr == kTx[i]);
            ok_m += (mr == kSl[i]);
            puts2("     sent "); putbin8(kTx[i]); puts2(" -> slave got "); putbin8(sr);
            puts2(" | slave sent "); putbin8(kSl[i]); puts2(" -> master got "); putbin8(mr);
            puts2("\r\n");
        }
        puts2("     match: slave "); putu2(ok_s); puts2("/4, master "); putu2(ok_m); puts2("/4\r\n");
    }
}

/* ---------------- 실험 B ---------------- */

static void Test_FlagOrder(void)
{
    SPI_Setup(0);
    while (!(SPI2->SR & SPI_SR_TXE)) {}
    SPI2->DR = 0x5A;

    uint32_t t_txe = 0, t_rxne = 0, t_bsy0 = 0, t_bsy1 = 0;
    uint32_t t0 = DWT->CYCCNT;
    SPI1->DR = 0x96;
    for (;;)
    {
        uint32_t s = SPI1->SR, t = DWT->CYCCNT - t0;
        if (!t_bsy1 && (s & SPI_SR_BSY))  t_bsy1 = t | 1u;      /* 0과 구별하려고 최하위 비트를 세운다 */
        if (!t_txe  && (s & SPI_SR_TXE))  t_txe  = t | 1u;
        if (!t_rxne && (s & SPI_SR_RXNE)) t_rxne = t | 1u;
        if (t_rxne && !t_bsy0 && !(s & SPI_SR_BSY)) t_bsy0 = t | 1u;
        if ((t_rxne && t_bsy0) || t > 500u * CYC_PER_US) break;
    }
    (void)SPI1->DR; (void)SPI2->DR;

    puts2("B) master full-duplex 1 byte at 250 kHz (8 bits = 32 us), time after DR write\r\n");
    puts2("   BSY=1 at "); putu2(t_bsy1 / CYC_PER_US);
    puts2(" us, TXE=1 at "); putu2(t_txe / CYC_PER_US);
    puts2(" us, RXNE=1 at "); putu2(t_rxne / CYC_PER_US);
    puts2(" us, BSY=0 at "); putu2(t_bsy0 / CYC_PER_US);
    puts2(" us\r\n");
    puts2("   (cycles: TXE "); putu2(t_txe); puts2(", RXNE "); putu2(t_rxne); puts2(", BSY=0 "); putu2(t_bsy0); puts2(")\r\n");
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    Log_Init();
    Pins_Init();

    puts2("\r\n07_SPI_Modes_BSY_Reg_c  (PA5->PB13, PA7->PB15, PB14->PA6)\r\n");
    for (uint32_t round = 1;; round++)
    {
        puts2("[round "); putu2(round); puts2("]\r\n");
        Test_Modes();
        Test_FlagOrder();
        delay_us(5000000u);
    }
}

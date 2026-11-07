/**
 * 03_DMA_Width_Error_Reg_c  —  [계층: 레지스터(CMSIS)]  DMA 데이터 폭 변환, EN=1 규칙, 전송 오류, 버스 공유를 실측
 *
 * 교안 「DMA」(주변장치 5) 6절의 실습이다. HAL 없이 DMA1 레지스터를 직접 다룬다. 인터럽트는 쓰지 않고 플래그를 폴링한다.
 *
 * 실험 A — 데이터 폭 변환 (RM0008 13.3.4, 표 76 "Programmable data width and endian behavior")
 *   메모리→메모리(CCR.MEM2MEM = 1, DIR = 0: CPAR 쪽에서 읽어 CMAR 쪽에 쓴다)로 PSIZE × MSIZE 9가지 조합을 돌린다.
 *   원본 = 바이트 0x00, 0x01, … 0x0F, NDT = 4, PINC = MINC = 1. 목적지 16바이트를 덤프해 표 76과 비교한다.
 *   좁은 쪽 → 넓은 쪽은 0으로 채우고, 넓은 쪽 → 좁은 쪽은 하위 비트만 남긴다(잘라 낸다).
 *
 * 실험 B — AHB→APB 다리에서의 복제 (RM0008 13.3.4 후반)
 *   DMA가 APB 주변장치에 바이트·하프워드로 쓰면 다리가 이를 32비트 쓰기로 바꾸며 값을 복제한다.
 *   TIM3->ARR(16비트)에 DMA로 0xAB(PSIZE = 8), 0x1234(PSIZE = 16)를 쓰고, ARR+1 주소에 0xCD 바이트를 쓴 뒤 다시 읽는다.
 *   기대: 0xABAB, 0x1234, 0xCDCD. 같은 실험을 CPU 바이트 쓰기로도 해 비교한다(메모리 맵 장 6.2절, 08_Clock_System/13).
 *
 * 실험 C — EN = 1인 동안의 규칙 (RM0008 13.4.4)
 *   CNDTR은 채널이 꺼져 있을 때만 쓸 수 있고, 켜진 뒤에는 읽기 전용(남은 개수)이다.
 *   요청이 없는 채널 4를 켠 상태에서 CNDTR에 3을 써도 10이 그대로인지 확인한다. 또 CNDTR = 0이면 MEM2MEM으로 켜도 전송이 없다.
 *
 * 실험 D — 전송 오류 (RM0008 13.3.5)
 *   예약 주소를 원본으로 주면 TEIF가 서고 하드웨어가 EN을 0으로 만든다. 주소 후보 두 개의 결과를 출력한다(실측 질문).
 *   정오표 ES096 2.4.1: 채널이 동작 중일 때 CGIF로 GIF를 지우면 자동 비활성화가 실패할 수 있으므로, 이 예제는 IFCR에서
 *   CTEIF·CHTIF·CTCIF 개별 비트만 쓴다. ES096 2.4.2: DMA 레지스터는 32비트 정렬 접근만 쓴다(구조체 접근은 32비트이다).
 *
 * 실험 E — 버스 공유 (RM0008 13.3 첫 단락, 3.1 BusMatrix 라운드 로빈)
 *   CPU가 SRAM 배열 합계를 내는 루프와, 레지스터만 쓰는 루프의 사이클을 DMA가 쉬고 있을 때와
 *   DMA가 SRAM을 채우고 있을 때(MEM2MEM, 32비트, 2048워드) 각각 DWT로 잰다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, AHB ÷1, APB1 ÷2(32 MHz) → TIM3 클럭 64 MHz
 *
 * 사용 레지스터 (RM0008 13.4절)
 *   DMA1_Channelx->CCR  : EN(0) TCIE(1) HTIE(2) TEIE(3) DIR(4) CIRC(5) PINC(6) MINC(7) PSIZE[9:8] MSIZE[11:10] PL[13:12] MEM2MEM(14)
 *   DMA1_Channelx->CNDTR: NDT[15:0]   CPAR / CMAR: 주소   DMA1->ISR / DMA1->IFCR: 채널별 GIF·TCIF·HTIF·TEIF (4비트씩)
 *   RCC->AHBENR.DMA1EN(bit0): DMA1 클럭
 *
 * 인터럽트를 사용하지 않는다(ISR·HAL 콜백 없음). 출력: USART2(PA2) 115200 8N1. 배선 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define CYC_PER_US  (SYSCLK_HZ / 1000000u)

/* CCR 필드 값 */
#define SZ8   0u
#define SZ16  1u
#define SZ32  2u
#define PL_VERYHIGH 3u

#define FILL_WORDS  2048u        /* 실험 E: DMA가 채울 SRAM 길이(8 KB) */
#define CPU_WORDS   512u         /* 실험 E: CPU가 합계를 낼 SRAM 길이(2 KB) */

static uint8_t  src8[16] __attribute__((aligned(4)));
static uint8_t  dst8[16] __attribute__((aligned(4)));
static uint32_t fill_dst[FILL_WORDS];
static uint32_t cpu_arr[CPU_WORDS];
static volatile uint32_t fill_word = 0xA5A5A5A5u;
static volatile uint32_t g_sink;

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 2 웨이트 스테이트(48~72 MHz) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;     /* HSI/2 × 16, APB1 ÷2 */
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

static void UART2_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 */
    USART2->BRR = (PCLK1_HZ + 115200u / 2u) / 115200u;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void uart_putc(char c) { while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) uart_putc(b[--i]);
}
static void uart_hex(uint32_t v, int digits)
{
    static const char h[] = "0123456789ABCDEF";
    while (digits--) uart_putc(h[(v >> (digits * 4)) & 0xFu]);
}

/* ---------------- DMA 보조 함수 ---------------- */

/* 채널 x(1..7)의 TEIF·HTIF·TCIF만 지운다(CGIF는 쓰지 않는다: ES096 2.4.1) */
static void dma_clear(uint32_t ch)
{
    uint32_t s = (ch - 1u) * 4u;
    DMA1->IFCR = (DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1) << s;
}

static uint32_t dma_flags(uint32_t ch) { return (DMA1->ISR >> ((ch - 1u) * 4u)) & 0xFu; }   /* bit0 GIF 1 TCIF 2 HTIF 3 TEIF */

/* 채널 1로 메모리→메모리 전송을 하고 끝날 때까지 기다린다. 반환: 채널 1의 ISR 4비트 */
static uint32_t m2m(uint32_t cpar, uint32_t cmar, uint32_t n, uint32_t psize, uint32_t msize, uint32_t extra)
{
    DMA1_Channel1->CCR   = 0;                        /* EN = 0일 때만 설정한다 */
    dma_clear(1);
    DMA1_Channel1->CPAR  = cpar;
    DMA1_Channel1->CMAR  = cmar;
    DMA1_Channel1->CNDTR = n;
    DMA1_Channel1->CCR   = DMA_CCR_MEM2MEM | (PL_VERYHIGH << 12) | (msize << 10) | (psize << 8) | extra;
    DMA1_Channel1->CCR  |= DMA_CCR_EN;               /* 켜는 순간 MEM2MEM 전송이 시작된다 */
    uint32_t t0 = DWT->CYCCNT;
    while (!(dma_flags(1) & 0xAu) && (DWT->CYCCNT - t0) < 10000u * CYC_PER_US) {}   /* TCIF 또는 TEIF */
    return dma_flags(1);
}

/* ---------------- 실험 ---------------- */

static void Test_Width(void)
{
    static const char *nm[3] = { " 8", "16", "32" };
    uart_puts("A) data width  src bytes 00..0F, NDT=4, PINC=MINC=1  (P = source/CPAR, M = dest/CMAR)\r\n");
    for (uint32_t p = 0; p < 3; p++)
        for (uint32_t m = 0; m < 3; m++)
        {
            for (uint32_t i = 0; i < 16; i++) { src8[i] = (uint8_t)i; dst8[i] = 0xEE; }
            uint32_t f = m2m((uint32_t)src8, (uint32_t)dst8, 4, p, m, DMA_CCR_PINC | DMA_CCR_MINC);
            uart_puts("   P");
            uart_puts(nm[p]);
            uart_puts(" -> M");
            uart_puts(nm[m]);
            uart_puts(" : ");
            for (uint32_t i = 0; i < 16; i++) { uart_hex(dst8[i], 2); uart_putc(' '); }
            uart_puts((f & 0x8u) ? "TE\r\n" : "\r\n");
        }
}

static void Test_ApbDuplicate(void)
{
    static uint8_t  b;
    static uint16_t h;
    uart_puts("B) DMA write to TIM3->ARR (16-bit APB1 register)\r\n");
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->CR1 = 0;                                         /* 카운터는 멈춘 상태 */

    TIM3->ARR = 0; b = 0xAB;
    m2m((uint32_t)&TIM3->ARR, (uint32_t)&b, 1, SZ8, SZ8, DMA_CCR_DIR);      /* DIR = 1: CMAR(메모리) → CPAR(ARR) */
    uart_puts("   DMA byte 0xAB  @ARR   -> ARR = 0x"); uart_hex(TIM3->ARR, 4); uart_puts("   (expect ABAB)\r\n");

    TIM3->ARR = 0; h = 0x1234;
    m2m((uint32_t)&TIM3->ARR, (uint32_t)&h, 1, SZ16, SZ16, DMA_CCR_DIR);
    uart_puts("   DMA half 0x1234 @ARR  -> ARR = 0x"); uart_hex(TIM3->ARR, 4); uart_puts("   (expect 1234)\r\n");

    TIM3->ARR = 0; b = 0xCD;
    m2m((uint32_t)&TIM3->ARR + 1u, (uint32_t)&b, 1, SZ8, SZ8, DMA_CCR_DIR);
    uart_puts("   DMA byte 0xCD  @ARR+1 -> ARR = 0x"); uart_hex(TIM3->ARR, 4); uart_puts("   (expect CDCD)\r\n");

    TIM3->ARR = 0;
    *(volatile uint8_t *)&TIM3->ARR = 0x5A;                /* 비교: CPU 바이트 쓰기 */
    uart_puts("   CPU byte 0x5A  @ARR   -> ARR = 0x"); uart_hex(TIM3->ARR, 4); uart_puts("\r\n");
    TIM3->ARR = 0xFFFF;
}

static void Test_EnRules(void)
{
    static uint32_t dummy_p, buf[16];
    uart_puts("C) EN=1 rules (channel 4: no peripheral request enabled)\r\n");
    DMA1_Channel4->CCR   = 0;
    dma_clear(4);
    DMA1_Channel4->CPAR  = (uint32_t)&dummy_p;
    DMA1_Channel4->CMAR  = (uint32_t)buf;
    DMA1_Channel4->CNDTR = 10;
    DMA1_Channel4->CCR   = (SZ32 << 10) | (SZ32 << 8) | DMA_CCR_MINC | DMA_CCR_EN;   /* 요청을 기다리는 상태 */
    DMA1_Channel4->CNDTR = 3;                              /* EN = 1: 쓰기가 무시되어야 한다 */
    uart_puts("   EN=1, wrote CNDTR=3 -> read "); uart_putu(DMA1_Channel4->CNDTR); uart_puts("   (expect 10)\r\n");
    DMA1_Channel4->CCR  &= ~DMA_CCR_EN;
    DMA1_Channel4->CNDTR = 3;
    uart_puts("   EN=0, wrote CNDTR=3 -> read "); uart_putu(DMA1_Channel4->CNDTR); uart_puts("   (expect 3)\r\n");

    /* CNDTR = 0이면 켜도 전송이 없다 */
    DMA1_Channel1->CCR   = 0;
    dma_clear(1);
    DMA1_Channel1->CPAR  = (uint32_t)src8;
    DMA1_Channel1->CMAR  = (uint32_t)dst8;
    DMA1_Channel1->CNDTR = 0;
    DMA1_Channel1->CCR   = DMA_CCR_MEM2MEM | DMA_CCR_PINC | DMA_CCR_MINC | DMA_CCR_EN;
    delay_us(100);
    uart_puts("   NDT=0 + MEM2MEM EN -> ISR flags(TE HT TC G) = "); uart_hex(dma_flags(1), 1); uart_puts("   (expect 0)\r\n");
    DMA1_Channel1->CCR = 0;
}

static void Test_Error(void)
{
    static const uint32_t addr[2] = { 0x40024000u, 0x60000000u };   /* AHB 예약 영역 후보, FSMC 영역(중밀도에는 FSMC 없음) */
    uart_puts("D) transfer error: read from a reserved address\r\n");
    for (uint32_t k = 0; k < 2; k++)
    {
        uint32_t f = m2m(addr[k], (uint32_t)dst8, 4, SZ32, SZ32, DMA_CCR_MINC);
        uart_puts("   src 0x"); uart_hex(addr[k], 8);
        uart_puts(" : TEIF="); uart_putu((f >> 3) & 1u);
        uart_puts(" TCIF="); uart_putu((f >> 1) & 1u);
        uart_puts(" EN after="); uart_putu(DMA1_Channel1->CCR & DMA_CCR_EN);
        uart_puts(" CNDTR left="); uart_putu(DMA1_Channel1->CNDTR);
        uart_puts("\r\n");
        DMA1_Channel1->CCR = 0;
        dma_clear(1);
    }
}

static uint32_t cpu_sram_loop(void)
{
    uint32_t s = 0, t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < CPU_WORDS; i++) s += cpu_arr[i];
    uint32_t c = DWT->CYCCNT - t0;
    g_sink = s;
    return c;
}

static uint32_t cpu_reg_loop(void)
{
    uint32_t a = 1, t0 = DWT->CYCCNT;
    for (uint32_t i = 0; i < CPU_WORDS; i++) { a = a * 3u + i; __asm volatile ("" : "+r"(a)); }
    uint32_t c = DWT->CYCCNT - t0;
    g_sink = a;
    return c;
}

/* DMA가 SRAM을 채우기 시작하게만 하고 바로 돌아온다 */
static void dma_fill_start(void)
{
    DMA1_Channel1->CCR   = 0;
    dma_clear(1);
    DMA1_Channel1->CPAR  = (uint32_t)&fill_word;          /* PINC = 0: 같은 워드를 반복해서 읽는다 */
    DMA1_Channel1->CMAR  = (uint32_t)fill_dst;
    DMA1_Channel1->CNDTR = FILL_WORDS;
    DMA1_Channel1->CCR   = DMA_CCR_MEM2MEM | (PL_VERYHIGH << 12) | (SZ32 << 10) | (SZ32 << 8) | DMA_CCR_MINC;
    DMA1_Channel1->CCR  |= DMA_CCR_EN;
}

static void dma_fill_wait(void)
{
    while (!(dma_flags(1) & 0xAu)) {}                     /* TCIF 또는 TEIF */
}

static void Test_BusSharing(void)
{
    uart_puts("E) bus sharing (cycles @64 MHz)\r\n");
    for (uint32_t i = 0; i < CPU_WORDS; i++) cpu_arr[i] = i;

    uint32_t s_idle = cpu_sram_loop();
    uint32_t r_idle = cpu_reg_loop();

    uint32_t t0 = DWT->CYCCNT;
    dma_fill_start();
    dma_fill_wait();
    uint32_t dma_alone = DWT->CYCCNT - t0;                 /* DMA 혼자 2048워드에 걸린 시간 */

    dma_fill_start();
    uint32_t s_busy = cpu_sram_loop();                     /* DMA가 SRAM을 쓰는 동안 CPU도 SRAM을 읽는다 */
    uint32_t still1 = DMA1_Channel1->CNDTR;
    dma_fill_wait();

    dma_fill_start();
    uint32_t r_busy = cpu_reg_loop();                      /* 레지스터만 쓰는 루프 */
    uint32_t still2 = DMA1_Channel1->CNDTR;
    dma_fill_wait();

    uart_puts("   DMA alone, 2048 words      : "); uart_putu(dma_alone); uart_puts(" cyc\r\n");
    uart_puts("   CPU SRAM sum 512 : idle "); uart_putu(s_idle); uart_puts("  / with DMA "); uart_putu(s_busy);
    uart_puts("   (DMA left "); uart_putu(still1); uart_puts(")\r\n");
    uart_puts("   CPU reg loop 512 : idle "); uart_putu(r_idle); uart_puts("  / with DMA "); uart_putu(r_busy);
    uart_puts("   (DMA left "); uart_putu(still2); uart_puts(")\r\n");
    DMA1_Channel1->CCR = 0;
}

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    UART2_Init();
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;                      /* DMA1 클럭 (AHB) */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);   /* PA5 = LD2 */

    uart_puts("\r\n03_DMA_Width_Error_Reg_c  (SYSCLK 64 MHz, polling, no IRQ)\r\n");
    while (1)
    {
        Test_Width();
        Test_ApbDuplicate();
        Test_EnRules();
        Test_Error();
        Test_BusSharing();
        uart_puts("\r\n");
        GPIOA->ODR ^= (1u << 5);
        delay_us(5000000u);
    }
}

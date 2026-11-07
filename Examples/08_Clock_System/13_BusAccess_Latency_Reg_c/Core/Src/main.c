/**
 * 13_BusAccess_Latency_Reg_c  —  [계층: 레지스터(CMSIS)]  같은 LDR/STR이라도 "어느 버스의 어느 주소"냐에 따라 걸리는 시간이 다르다
 *
 * 교안 「메모리 맵과 버스(AHB/APB)」 4절(버스 클럭과 접근 시간)·6절(접근 폭)의 실습이다.
 *
 * 배경 (RM0008 Rev 21 3.1 System architecture, 3.3.3 Embedded Flash memory)
 *   Cortex-M3 의 데이터 접근은 System 버스 → 버스 행렬(BusMatrix) → {SRAM, FLITF(Flash), AHB→APB 브리지 2개} 로 간다.
 *   - SRAM      : 버스 행렬에 직접 붙은 AHB 슬레이브. 대기 없음.
 *   - Flash     : FLITF 를 거친다. SYSCLK 64 MHz 에서는 FLASH_ACR.LATENCY = 010(2 웨이트 스테이트)이어야 한다.
 *                 데이터 읽기(DCode)에는 프리페치 버퍼가 쓰이지 않는다(프리페치는 ICode 명령어 인출용).
 *   - APB2 GPIO : AHB→APB2 브리지를 지난다. 이 설정에서 PCLK2 = HCLK = 64 MHz.
 *   - APB1 TIM2 : AHB→APB1 브리지를 지난다. PCLK1 = HCLK/2 = 32 MHz 이므로 APB1 한 사이클 = HCLK 두 사이클.
 *   쓰기는 코어의 쓰기 버퍼에 들어가면 CPU가 먼저 진행할 수 있어(posted write) 읽기와 다른 숫자가 나온다.
 *
 * 실험 A — 읽기·쓰기 접근 비용 (DWT->CYCCNT)
 *   대상마다 volatile 접근을 8회 펼친 루프와 16회 펼친 루프를 N번 돌리고,
 *   (t16 - t8) / (8·N) 로 "접근 1회당 HCLK 사이클"을 구한다. 루프 오버헤드는 두 측정에 똑같이 들어가 상쇄된다.
 *   대상: SRAM 변수, Flash 상수, GPIOA->IDR(APB2), TIM2->CNT(APB1) 읽기 / SRAM, GPIOB->BSRR(APB2), TIM2->CCR1(APB1) 쓰기
 *   값은 컴파일러 최적화 수준에 따라 조금 달라질 수 있으므로 "상대 비교"로 읽는다.
 *
 * 실험 B — APB 브리지의 8·16비트 → 32비트 복제 (RM0008 3.1 Note)
 *   "APB 레지스터에 16비트나 8비트로 접근하면 32비트 접근으로 바뀌며, 브리지가 16·8비트 데이터를 복제해 32비트 벡터를 만든다."
 *   (1) GPIOB->ODR 의 "상위 하프워드 주소"(+2)에 0x00A5 를 16비트로 쓴다 → 복제되면 하위 16비트(ODR)에도 0x00A5 가 들어간다.
 *       GPIO 레지스터는 원래 32비트 접근만 허용(RM0008 9.2)하므로, 이 실험은 "규칙을 어기면 무슨 일이 일어나는가"를 보는 것이다.
 *       PB 핀은 리셋값(입력 플로팅)이라 ODR 이 바뀌어도 핀 상태에는 영향이 없다.
 *   (2) TIM2->CCR1 의 상위 바이트 주소(+1)에 0x12 를 8비트로 쓴다 → 복제되면 CCR1 = 0x1212 가 된다.
 *       타이머 레지스터는 바이트 쓰기가 허용되지 않는다(RM0008 15.4: 하프워드·워드 쓰기만).
 *   기대값은 매뉴얼 문장에서 끌어낸 가설이다. 실측값으로 확인한다.
 *
 * 실험 C — 비트 밴딩 별칭 주소 (RM0008 3.3.2, PM0056 2.2.5)
 *   alias = 0x42000000 + (offset × 32) + (bit × 4). GPIOA_ODR bit5 → 0x42210194, RCC_APB2ENR bit2(IOPAEN) → 0x42420308.
 *   별칭을 읽은 값이 실제 비트와 같은지 확인한다.
 *
 * 클럭: HSI/2 × 16 = SYSCLK 64 MHz, AHB ÷1, APB1 ÷2(32 MHz), APB2 ÷1(64 MHz), FLASH_ACR.LATENCY = 2
 *
 * 사용 레지스터·비트
 *   FLASH->ACR : LATENCY[2:0], PRFTBE(bit4)                 (RM0008 3.3.3)
 *   RCC->CFGR, RCC->CR : PLL 64 MHz 설정                    (RM0008 7.3)
 *   RCC->APB2ENR : IOPAEN(bit2), IOPBEN(bit3) / RCC->APB1ENR : TIM2EN(bit0), USART2EN(bit17)
 *   GPIOA->IDR, GPIOB->BSRR, GPIOB->ODR, TIM2->CNT, TIM2->CCR1
 *   DWT->CYCCNT, DWT->CTRL.CYCCNTENA, CoreDebug->DEMCR.TRCENA
 *
 * ISR: 사용하지 않는다. 출력: USART2(PA2, ST-LINK 가상 COM) 115200 8N1. 배선 없음.
 */
#include "stm32f1xx.h"

#define SYSCLK_HZ   64000000u
#define PCLK1_HZ    32000000u
#define N_LOOP      256u

#define REP8(x)  x; x; x; x; x; x; x; x
#define REP16(x) REP8(x); REP8(x)

static volatile uint32_t g_sram_word = 0x12345678u;                 /* SRAM (.data) */
static const uint32_t g_flash_word = 0xCAFEF00Du;                   /* Flash (.rodata). const volatile 로 선언하면 GCC가 .data(SRAM)에 둔다 */
#define FLASH_WORD (*(const volatile uint32_t *)&g_flash_word)       /* 읽기는 volatile 포인터로: 매번 실제로 Flash를 읽게 한다 */

/* ---------------- 공통: 클럭, DWT, UART ---------------- */

static void Clock_Init64MHz(void)
{
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;       /* 48 MHz < SYSCLK ≤ 72 MHz → 2 웨이트 스테이트 먼저 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2;     /* HSI/2 × 16, AHB ÷1, APB1 ÷2, APB2 ÷1 */
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

static void UART2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 8)) | (0xBu << 8);   /* PA2 = USART2_TX, AF 푸시풀 50 MHz */
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
static void uart_hex(uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(hx[(v >> i) & 0xFu]);
}
/* 사이클 × 100 값을 "정수.소수2자리"로 출력 */
static void uart_put_x100(uint32_t v100)
{
    uart_putu(v100 / 100u); uart_putc('.');
    uart_putc((char)('0' + (v100 / 10u) % 10u)); uart_putc((char)('0' + v100 % 10u));
}

/* ---------------- 실험 A: 접근 비용 ---------------- */

/* noinline: 측정 함수마다 같은 모양의 루프가 되도록 */
#define DEF_READ(name, expr)                                                              \
    static __attribute__((noinline)) uint32_t name##_8(void)                              \
    { uint32_t t0 = DWT->CYCCNT; for (uint32_t i = 0; i < N_LOOP; i++) { REP8((void)(expr)); } return DWT->CYCCNT - t0; } \
    static __attribute__((noinline)) uint32_t name##_16(void)                             \
    { uint32_t t0 = DWT->CYCCNT; for (uint32_t i = 0; i < N_LOOP; i++) { REP16((void)(expr)); } return DWT->CYCCNT - t0; }

#define DEF_WRITE(name, lhs)                                                              \
    static __attribute__((noinline)) uint32_t name##_8(void)                              \
    { uint32_t t0 = DWT->CYCCNT; for (uint32_t i = 0; i < N_LOOP; i++) { REP8((lhs) = 0u); } return DWT->CYCCNT - t0; } \
    static __attribute__((noinline)) uint32_t name##_16(void)                             \
    { uint32_t t0 = DWT->CYCCNT; for (uint32_t i = 0; i < N_LOOP; i++) { REP16((lhs) = 0u); } return DWT->CYCCNT - t0; }

DEF_READ(rd_sram,  g_sram_word)
DEF_READ(rd_flash, FLASH_WORD)
DEF_READ(rd_apb2,  GPIOA->IDR)
DEF_READ(rd_apb1,  TIM2->CNT)
DEF_WRITE(wr_sram, g_sram_word)
DEF_WRITE(wr_apb2, GPIOB->BSRR)        /* 0을 쓰면 어떤 비트도 바뀌지 않는다 */
DEF_WRITE(wr_apb1, TIM2->CCR1)         /* TIM2는 멈춰 있고 출력도 꺼져 있다 */

static void Report(const char *label, uint32_t (*f8)(void), uint32_t (*f16)(void))
{
    uint32_t a = f8(), b = f16();
    uint32_t per100 = ((b - a) * 100u) / (8u * N_LOOP);
    uart_puts("  ");
    uart_puts(label);
    uart_put_x100(per100);
    uart_puts(" cycles\r\n");
}

/* ---------------- main ---------------- */

int main(void)
{
    Clock_Init64MHz();
    DWT_Init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;   /* GPIOA·B 클럭: 켜기 전 쓰기는 무시된다 */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    UART2_Init();
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);  /* PA5 = LD2 */

    uart_puts("\r\n13_BusAccess_Latency_Reg_c  SYSCLK=HCLK=PCLK2=64 MHz, PCLK1=32 MHz, Flash LATENCY=");
    uart_putu(FLASH->ACR & FLASH_ACR_LATENCY);
    uart_puts("\r\n  &g_sram_word = "); uart_hex((uint32_t)&g_sram_word);      /* 0x2000xxxx 이어야 SRAM */
    uart_puts("  &g_flash_word = "); uart_hex((uint32_t)&g_flash_word);          /* 0x0800xxxx 이어야 Flash */
    uart_puts("\r\n");

    for (uint32_t round = 1;; round++)
    {
        uart_puts("[round "); uart_putu(round); uart_puts("]\r\n");

        uart_puts("A) cycles per access (HCLK)\r\n");
        Report("read  SRAM  variable        : ", rd_sram_8,  rd_sram_16);
        Report("read  Flash const (LAT=2)   : ", rd_flash_8, rd_flash_16);
        Report("read  APB2  GPIOA->IDR      : ", rd_apb2_8,  rd_apb2_16);
        Report("read  APB1  TIM2->CNT       : ", rd_apb1_8,  rd_apb1_16);
        Report("write SRAM  variable        : ", wr_sram_8,  wr_sram_16);
        Report("write APB2  GPIOB->BSRR     : ", wr_apb2_8,  wr_apb2_16);
        Report("write APB1  TIM2->CCR1      : ", wr_apb1_8,  wr_apb1_16);

        uart_puts("B) APB bridge 8/16-bit -> 32-bit duplication\r\n");
        GPIOB->ODR = 0u;
        *(volatile uint16_t *)((uint32_t)&GPIOB->ODR + 2u) = 0x00A5u;   /* 상위 하프워드 주소에 16비트 쓰기 */
        uart_puts("  GPIOB->ODR after 16-bit write 0x00A5 to (ODR+2): ");
        uart_hex(GPIOB->ODR);
        uart_puts("   (hypothesis: 0x000000A5)\r\n");
        GPIOB->ODR = 0u;

        TIM2->CCR1 = 0u;
        *(volatile uint8_t *)((uint32_t)&TIM2->CCR1 + 1u) = 0x12u;       /* 상위 바이트 주소에 8비트 쓰기 */
        uart_puts("  TIM2->CCR1 after 8-bit write 0x12 to (CCR1+1)  : ");
        uart_hex(TIM2->CCR1);
        uart_puts("   (hypothesis: 0x00001212)\r\n");
        TIM2->CCR1 = 0u;

        uart_puts("C) bit-band alias\r\n");
        volatile uint32_t *bb_odr5   = (volatile uint32_t *)(PERIPH_BB_BASE + ((uint32_t)&GPIOA->ODR - PERIPH_BASE) * 32u + 5u * 4u);
        volatile uint32_t *bb_iopaen = (volatile uint32_t *)(PERIPH_BB_BASE + ((uint32_t)&RCC->APB2ENR - PERIPH_BASE) * 32u + 2u * 4u);
        *bb_odr5 = 1u;                                                   /* 별칭에 1 → LD2 켜짐 */
        uart_puts("  alias(GPIOA_ODR bit5)   = "); uart_hex((uint32_t)bb_odr5);
        uart_puts("  read "); uart_putu(*bb_odr5);
        uart_puts("  ODR="); uart_hex(GPIOA->ODR); uart_puts("\r\n");
        uart_puts("  alias(RCC_APB2ENR bit2) = "); uart_hex((uint32_t)bb_iopaen);
        uart_puts("  read "); uart_putu(*bb_iopaen); uart_puts("  (expect 1)\r\n");
        for (volatile uint32_t d = 0; d < 6400000u; d++) {}              /* 약 0.5 s 이상 */
        *bb_odr5 = 0u;                                                   /* LD2 꺼짐 */

        for (volatile uint32_t d = 0; d < 20000000u; d++) {}
    }
}

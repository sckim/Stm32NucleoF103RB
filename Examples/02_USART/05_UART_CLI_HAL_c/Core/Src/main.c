/**
 * 05_UART_CLI_HAL_c  —  [계층: HAL]  UART 명령줄 인터페이스(CLI): 한 줄 입력 편집 + 명령어 테이블
 *
 * 터미널(115200 8N1, "Enter = CR 또는 LF")에서 아래 명령을 입력한다. Backspace(0x08/0x7F) 편집 지원.
 *   help                     명령 목록
 *   led on | off | toggle    LD2(PA5) 제어
 *   uptime                   부팅 후 경과 시간 (HAL_GetTick 기반)
 *   echo <text...>           입력 문자열 그대로 출력
 *   peek <hex_addr>          32비트 워드 읽기 (주소는 4바이트 정렬, 예: peek 0x40010800 = GPIOA->CRL)
 *   reset                    NVIC_SystemReset() 로 소프트웨어 리셋
 *
 * 설계
 *   1) 수신: 인터럽트가 1바이트씩 링 버퍼(04_USART_RingBuffer 예제와 같은 방식)에 넣는다.
 *   2) 줄 조립: main 이 한 글자씩 꺼내 라인 버퍼에 쌓고 에코/백스페이스 처리, Enter 에서 한 줄 완성
 *   3) 파싱: 공백으로 토큰 분리(strtok) -> 명령 테이블(이름, 핸들러)에서 검색 -> 핸들러 호출
 *   명령을 추가하려면 핸들러 함수를 만들고 cmd_table 에 한 줄 추가한다.
 *   * peek 은 읽기 전용이다. 잘못된(존재하지 않는) 주소를 읽으면 BusFault -> HardFault 가 나므로 주의
 *     (진단 방법은 08_Clock_System/05_HardFault_Diagnosis 참고)
 *
 * ISR vs 콜백
 *   USART2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler() -> HAL_UART_RxCpltCallback() (이 파일)
 */
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RB_SIZE   64U
#define RB_MASK   (RB_SIZE - 1U)
#define LINE_MAX  64U
#define MAX_ARGS  6U

UART_HandleTypeDef huart2;

static uint8_t rb_buf[RB_SIZE];
static volatile uint32_t rb_head, rb_tail;
static uint8_t rx_byte;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

static void out(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

/* ---------------- 명령 핸들러 ---------------- */
typedef void (*cmd_fn)(int argc, char **argv);

static void cmd_help(int argc, char **argv);
static void cmd_led(int argc, char **argv)
{
    (void)argc;
    if (argc < 2) { out("usage: led on|off|toggle\r\n"); return; }
    if (!strcmp(argv[1], "on"))          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    else if (!strcmp(argv[1], "off"))    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    else if (!strcmp(argv[1], "toggle")) HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    else { out("usage: led on|off|toggle\r\n"); return; }
    out("ok\r\n");
}
static void cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    char m[48];
    uint32_t t = HAL_GetTick();
    snprintf(m, sizeof m, "%lu.%03lu s\r\n", (unsigned long)(t / 1000), (unsigned long)(t % 1000));
    out(m);
}
static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) { out(argv[i]); out(i + 1 < argc ? " " : ""); }
    out("\r\n");
}
static void cmd_peek(int argc, char **argv)
{
    if (argc < 2) { out("usage: peek <hex_addr>\r\n"); return; }
    uint32_t addr = (uint32_t)strtoul(argv[1], NULL, 16);
    if (addr & 3U) { out("address must be 4-byte aligned\r\n"); return; }
    char m[48];
    snprintf(m, sizeof m, "[0x%08lX] = 0x%08lX\r\n", (unsigned long)addr, (unsigned long)*(volatile uint32_t *)addr);
    out(m);
}
static void cmd_reset(int argc, char **argv) { (void)argc; (void)argv; out("resetting...\r\n"); HAL_Delay(20); NVIC_SystemReset(); }

static const struct { const char *name; cmd_fn fn; const char *help; } cmd_table[] = {
    { "help",   cmd_help,   "명령 목록" },
    { "led",    cmd_led,    "led on|off|toggle" },
    { "uptime", cmd_uptime, "부팅 후 경과 시간" },
    { "echo",   cmd_echo,   "echo <text>" },
    { "peek",   cmd_peek,   "peek <hex_addr>  (32비트 읽기)" },
    { "reset",  cmd_reset,  "소프트웨어 리셋" },
};
#define N_CMDS (sizeof cmd_table / sizeof cmd_table[0])

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    for (unsigned i = 0; i < N_CMDS; i++) { out("  "); out(cmd_table[i].name); out(" - "); out(cmd_table[i].help); out("\r\n"); }
}

/* 한 줄을 토큰으로 나눠 실행 */
static void execute(char *line)
{
    char *argv[MAX_ARGS];
    int argc = 0;
    for (char *t = strtok(line, " \t"); t && argc < (int)MAX_ARGS; t = strtok(NULL, " \t")) argv[argc++] = t;
    if (argc == 0) return;

    for (unsigned i = 0; i < N_CMDS; i++)
    {
        if (!strcmp(argv[0], cmd_table[i].name)) { cmd_table[i].fn(argc, argv); return; }
    }
    out("unknown command (type 'help')\r\n");
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    out("\r\n[UART CLI] type 'help'\r\n> ");
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    char line[LINE_MAX];
    uint32_t len = 0;
    while (1)
    {
        if (rb_head == rb_tail) continue;                 /* 수신 데이터 없음 */
        uint8_t c = rb_buf[rb_tail];
        rb_tail = (rb_tail + 1U) & RB_MASK;

        if (c == '\r' || c == '\n')                       /* 줄 완성 */
        {
            out("\r\n");
            line[len] = '\0';
            execute(line);
            len = 0;
            out("> ");
        }
        else if (c == 0x08 || c == 0x7F)                  /* 백스페이스: 화면에서 한 글자 지우기 "\b \b" */
        {
            if (len > 0) { len--; out("\b \b"); }
        }
        else if (c >= 0x20 && c < 0x7F && len < LINE_MAX - 1U)   /* 출력 가능한 문자만 */
        {
            line[len++] = (char)c;
            HAL_UART_Transmit(&huart2, &c, 1, 10);        /* 에코 */
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HAL 콜백 (ISR 문맥): 수신 바이트를 링 버퍼에 넣고 재무장 (가득 차면 버림)         */
/* ------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    uint32_t next = (rb_head + 1U) & RB_MASK;
    if (next != rb_tail) { rb_buf[rb_head] = rx_byte; rb_head = next; }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

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
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
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

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_5;  g.Mode = GPIO_MODE_OUTPUT_PP;  g.Pull = GPIO_NOPULL;  g.Speed = GPIO_SPEED_FREQ_LOW;   /* LD2 */
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

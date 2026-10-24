/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/**
 * 05_UART_CLI_c  —  [계층: HAL]  UART 명령줄 인터페이스(CLI): 한 줄 입력 편집 + 명령어 테이블
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
 *     (진단 방법은 08_Clock_System/08_HardFault_Diagnosis 참고)
 *
 * ISR vs 콜백
 *   USART2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler() -> HAL_UART_RxCpltCallback() (이 파일)
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RB_SIZE   64U
#define RB_MASK   (RB_SIZE - 1U)
#define LINE_MAX  64U
#define MAX_ARGS  6U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t rb_buf[RB_SIZE];
static volatile uint32_t rb_head, rb_tail;
static uint8_t rx_byte;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void out(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

/* ---------------- 명령 핸들러 ---------------- */
typedef void (*cmd_fn)(int argc, char **argv);

static void cmd_help(int argc, char **argv);
static void cmd_led(int argc, char **argv)
{
    (void)argc;
    if (argc < 2) { out("usage: led on|off|toggle\r\n"); return; }
    if (!strcmp(argv[1], "on"))          HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    else if (!strcmp(argv[1], "off"))    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    else if (!strcmp(argv[1], "toggle")) HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
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

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char line[LINE_MAX];
  uint32_t len = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  out("\r\n[UART CLI] type 'help'\r\n> ");
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
      HAL_UART_Transmit(&huart2, &c, 1, 10);          /* 에코 */
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
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
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

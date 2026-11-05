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
 * 07_USART_Stack  —  [계층: HAL + CMSIS 내장 함수]  스택(LIFO) 자료구조: 괄호 검사기 + ISR 공유 시 경쟁 상태 실험
 *
 * 교안: 이 폴더의 "스택 교안.html" (짝 교안: 03_USART_RingBuffer/"UART 링 버퍼 교안.html")
 *
 * 구조
 *   수신 : 03_USART_RingBuffer 와 같은 링 버퍼(FIFO). ISR 이 push, main 이 pop  -> 받은 순서 유지
 *   검사 : main 이 꺼낸 글자를 스택(LIFO)으로 괄호 검사. 여는 괄호 push, 닫는 괄호 pop 후 짝 비교
 *   스택 : 배열 + top 하나. top = "다음에 쓸 빈 칸" = 저장된 개수.  empty: top == 0,  full: top == size
 *          링 버퍼와 달리 되감기가 없어 N 칸을 모두 쓰고, 크기가 2 의 거듭제곱일 필요도 없다.
 *
 * 시험 방법 (터미널 115200 8N1)
 *   - 괄호가 섞인 한 줄을 입력하고 엔터: "OK" 또는 mismatch / underflow / unclosed / overflow 와 위치(col) 출력
 *     예) {a[b(c)d]e} -> OK,  ([)] -> mismatch,  (a)) -> underflow,  ((a) -> unclosed,  9단 중첩 -> overflow (BR_STK_SIZE=8)
 *   - B1(PC13): 경쟁 상태 실험. SysTick ISR(1ms) 이 공유 스택에 push 하는 동안 main 이 같은 스택에 push/pop 을 반복한다.
 *       ① 보호 없음     : main 의 top 읽기-수정-쓰기 사이에 ISR 이 끼어들면 ISR 이 넣은 항목이 사라진다 (lost > 0 기대)
 *       ② PRIMASK 보호  : 같은 동작을 임계 구역으로 감싸면 lost = 0
 *
 * 하드웨어 대응
 *   USART2->CR1.RXNEIE, SR.RXNE, DR (HAL_UART_Receive_IT 가 관리). NVIC USART2_IRQn.
 *   SysTick (SysTick->CTRL/LOAD/VAL, HAL_Init 이 1ms 로 설정). PRIMASK 특수 레지스터 bit0 (1 = 설정 가능한 예외 모두 차단).
 *
 * ISR vs 콜백
 *   USART2_IRQHandler (stm32f1xx_it.c, 진짜 ISR) -> HAL_UART_IRQHandler() -> HAL_UART_RxCpltCallback() (이 파일, ISR 문맥: 링 버퍼 push)
 *   SysTick_Handler   (stm32f1xx_it.c, 진짜 ISR) -> HAL_IncTick() 다음 USER CODE 영역에서 Stack_RaceTick() (이 파일, ISR 문맥: 공유 스택 push)
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define RB_SIZE 128U                      /* 링 버퍼: 반드시 2 의 거듭제곱 */
#define RB_MASK (RB_SIZE - 1U)

typedef struct {
    volatile uint32_t head;               /* 다음에 쓸 위치 (생산자=ISR 만 수정) */
    volatile uint32_t tail;               /* 다음에 읽을 위치 (소비자=main 만 수정) */
    uint8_t buf[RB_SIZE];
} ringbuf_t;

/* 스택. 이름을 stack_t 로 하지 않는다 (newlib <signal.h> 의 stack_t 와 충돌) */
typedef struct {
    uint8_t *buf;                         /* 저장 공간 (외부 배열) */
    uint32_t size;                        /* 칸 수 N. 2 의 거듭제곱일 필요 없음 */
    volatile uint32_t top;                /* 다음에 쓸 빈 칸 = 저장된 개수. ISR 과 공유할 수 있으므로 volatile */
    uint32_t max_depth;                   /* 학습용 통계: 최대 깊이 */
} stk_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BR_STK_SIZE   8U                  /* 괄호 검사 스택: 교안 시뮬레이터 기본값과 같은 8칸 (9단 중첩이면 overflow) */
#define RACE_STK_SIZE 1024U               /* 경쟁 실험 스택: ISR 이 RACE_MS 동안 1ms 마다 1개씩 쌓아도 넘치지 않는 크기 */
#define RACE_MS       500U                /* 실험 한 번의 길이 (ms) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static ringbuf_t rb;
static uint8_t rx_byte;
static volatile uint32_t dropped;         /* 링 버퍼가 가득 차 버린 바이트 수 */

static uint8_t br_mem[BR_STK_SIZE];
static stk_t   br_stk = { br_mem, BR_STK_SIZE, 0U, 0U };      /* main 전용: 괄호 검사 */

static uint8_t race_mem[RACE_STK_SIZE];
static stk_t   race_stk = { race_mem, RACE_STK_SIZE, 0U, 0U }; /* SysTick ISR 과 main 이 공유 */
static volatile uint8_t  race_active;     /* 1 이면 SysTick ISR 이 race_stk 에 push */
static volatile uint32_t race_isr_push;   /* ISR 이 push 에 성공한 횟수 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ------------------------------------------------------------------------- */
/* 링 버퍼 (03_USART_RingBuffer 와 동일)                                         */
/* ------------------------------------------------------------------------- */
/* 생산자 측 (ISR 에서 호출). 가득 차면 새 바이트를 버리고 0 반환 */
static int rb_push(ringbuf_t *r, uint8_t b)
{
  uint32_t next = (r->head + 1U) & RB_MASK;
  if (next == r->tail) return 0;        /* full */
  r->buf[r->head] = b;
  r->head = next;                       /* 데이터를 쓴 "뒤에" head 갱신 */
  return 1;
}

/* 소비자 측 (main). 비어 있으면 0 반환 */
static int rb_pop(ringbuf_t *r, uint8_t *b)
{
  if (r->head == r->tail) return 0;     /* empty */
  *b = r->buf[r->tail];
  r->tail = (r->tail + 1U) & RB_MASK;
  return 1;
}

/* ------------------------------------------------------------------------- */
/* 스택 (LIFO). top = 다음 빈 칸.  push: 검사 -> 쓰기 -> top++,  pop: 검사 -> top-- -> 읽기 */
/* ------------------------------------------------------------------------- */
static void stk_reset(stk_t *s) { s->top = 0U; s->max_depth = 0U; }

static int stk_push(stk_t *s, uint8_t v)
{
  if (s->top >= s->size) return 0;     /* overflow: buf[size] 는 배열 바깥 */
  s->buf[s->top] = v;
  s->top++;                             /* LDR -> ADDS -> STR 세 단계 (원자적이지 않음) */
  if (s->top > s->max_depth) s->max_depth = s->top;
  return 1;
}

static int stk_pop(stk_t *s, uint8_t *v)
{
  if (s->top == 0U) return 0;           /* underflow: 꺼낼 것이 없다 */
  s->top--;                             /* LDR -> SUBS -> STR 세 단계 (원자적이지 않음) */
  *v = s->buf[s->top];
  return 1;
}

static int stk_peek(const stk_t *s, uint8_t *v)
{
  if (s->top == 0U) return 0;
  *v = s->buf[s->top - 1U];             /* top 은 바꾸지 않는다 */
  return 1;
}

/* ISR 과 공유하는 스택용: PRIMASK 를 저장 -> 차단 -> 복원 (무조건 __enable_irq() 하지 않는다) */
static int stk_push_safe(stk_t *s, uint8_t v)
{
  uint32_t primask = __get_PRIMASK();   /* 현재 PRIMASK 저장 (호출 전에 이미 막혀 있었을 수 있음) */
  __disable_irq();                      /* CPSID i: PRIMASK = 1, NMI/HardFault 외 모든 예외 차단 */
  int ok = stk_push(s, v);
  __set_PRIMASK(primask);               /* 원래 상태로 복원 */
  return ok;
}

static int stk_pop_safe(stk_t *s, uint8_t *v)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  int ok = stk_pop(s, v);
  __set_PRIMASK(primask);
  return ok;
}

/* ------------------------------------------------------------------------- */
/* UART 출력 도우미 (main 에서만 호출. ISR 안에서는 전송하지 않는다)                 */
/* ------------------------------------------------------------------------- */
static void uart_puts(const char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* ------------------------------------------------------------------------- */
/* 괄호 검사기: 한 줄을 한 글자씩 받아 스택으로 검사                                */
/* ------------------------------------------------------------------------- */
typedef enum { BR_OK = 0, BR_OVERFLOW, BR_UNDERFLOW, BR_MISMATCH, BR_UNCLOSED } br_err_t;

static br_err_t br_err;                   /* 이 줄의 첫 오류 (BR_OK 면 아직 오류 없음) */
static uint32_t br_col;                   /* 현재 글자 위치 (1부터) */
static uint32_t br_err_col;               /* 첫 오류 위치 */
static uint8_t  br_got, br_open;          /* mismatch 일 때 닫는 괄호 / 스택에서 꺼낸 여는 괄호 */

static uint8_t br_pair_of(uint8_t close)  /* 닫는 괄호에 짝이 되는 여는 괄호 */
{
  return (close == ')') ? '(' : (close == ']') ? '[' : '{';
}

static void br_feed(uint8_t c)
{
  br_col++;
  if (br_err != BR_OK) return;          /* 첫 오류 이후는 검사하지 않는다 (에코만) */

  if (c == '(' || c == '[' || c == '{')
  {
    if (!stk_push(&br_stk, c)) { br_err = BR_OVERFLOW; br_err_col = br_col; }
  }
  else if (c == ')' || c == ']' || c == '}')
  {
    uint8_t o;
    if (!stk_pop(&br_stk, &o))      { br_err = BR_UNDERFLOW; br_err_col = br_col; }
    else if (o != br_pair_of(c))    { br_err = BR_MISMATCH;  br_err_col = br_col; br_open = o; br_got = c; }
  }
  /* 괄호가 아닌 글자는 무시 */
}

static void br_finish_line(char *msg, size_t len)
{
  uint8_t t;
  if (br_err == BR_OK && stk_peek(&br_stk, &t)) { br_err = BR_UNCLOSED; br_open = t; }

  switch (br_err)
  {
  case BR_OK:
    snprintf(msg, len, "\r\n  OK (max_depth=%lu/%lu)\r\n",
             (unsigned long)br_stk.max_depth, (unsigned long)br_stk.size);
    break;
  case BR_OVERFLOW:
    snprintf(msg, len, "\r\n  overflow at col %lu: nesting deeper than %lu\r\n",
             (unsigned long)br_err_col, (unsigned long)br_stk.size);
    break;
  case BR_UNDERFLOW:
    snprintf(msg, len, "\r\n  underflow at col %lu: no open bracket to close\r\n",
             (unsigned long)br_err_col);
    break;
  case BR_MISMATCH:
    snprintf(msg, len, "\r\n  mismatch at col %lu: '%c' closed by '%c'\r\n",
             (unsigned long)br_err_col, br_open, br_got);
    break;
  default: /* BR_UNCLOSED */
    snprintf(msg, len, "\r\n  unclosed: %lu open bracket(s) left, top='%c'\r\n",
             (unsigned long)br_stk.top, br_open);
    break;
  }
  uart_puts(msg);

  stk_reset(&br_stk);                   /* 다음 줄 준비 */
  br_err = BR_OK; br_col = 0U;
}

/* ------------------------------------------------------------------------- */
/* 경쟁 상태 실험: ISR 이 push 하는 동안 main 도 같은 top 을 push/pop 으로 수정        */
/*   main 의 push/pop 은 짝을 이뤄 순증감 0 이므로, 정상이라면 끝난 뒤 top == ISR push 횟수 */
/*   lost = ISR push 횟수 - top  (읽기-수정-쓰기가 끊겨 사라진 항목 수)                */
/* ------------------------------------------------------------------------- */
static uint32_t race_run(int protect, uint32_t *loops)
{
  uint8_t d;
  uint32_t n = 0U;

  stk_reset(&race_stk);
  race_isr_push = 0U;
  race_active = 1U;                     /* 이제부터 SysTick ISR 이 push 한다 */

  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < RACE_MS)
  {
    if (protect) { stk_push_safe(&race_stk, 'M'); stk_pop_safe(&race_stk, &d); }
    else         { stk_push(&race_stk, 'M');      stk_pop(&race_stk, &d); }
    n++;
  }

  race_active = 0U;                     /* 이후 ISR 은 아무것도 하지 않으므로 아래 값은 고정 */
  *loops = n;
  return race_isr_push - race_stk.top;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char msg[96];
  uint8_t c;
  uint32_t line_len = 0U;               /* 현재 줄에 들어온 글자 수 (빈 줄 무시용) */
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
  uart_puts("\r\n[stack demo] type a line with ()[]{} and press Enter. B1 = ISR race test\r\n");
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* B1: 경쟁 상태 실험 (보호 없음 -> PRIMASK 보호 순서) */
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* GPIOC->IDR bit13 (B1: 누르면 Low) */
    {
      uint32_t loops, lost;
      uart_puts("\r\n[race test] SysTick ISR pushes every 1ms, main pushes/pops the same stack\r\n");

      lost = race_run(0, &loops);
      snprintf(msg, sizeof msg, "  unprotected : isr_push=%lu, top=%lu, lost=%lu (main loops=%lu)\r\n",
               (unsigned long)race_isr_push, (unsigned long)race_stk.top,
               (unsigned long)lost, (unsigned long)loops);
      uart_puts(msg);

      lost = race_run(1, &loops);
      snprintf(msg, sizeof msg, "  PRIMASK     : isr_push=%lu, top=%lu, lost=%lu (main loops=%lu)\r\n",
               (unsigned long)race_isr_push, (unsigned long)race_stk.top,
               (unsigned long)lost, (unsigned long)loops);
      uart_puts(msg);

      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
    }

    /* 링 버퍼(FIFO)에서 받은 순서대로 꺼내 스택(LIFO)으로 괄호 검사 */
    while (rb_pop(&rb, &c))
    {
      if (c == '\r' || c == '\n')
      {
        if (line_len > 0U) br_finish_line(msg, sizeof msg);   /* CR+LF 의 두 번째 글자 같은 빈 줄은 무시 */
        line_len = 0U;
      }
      else
      {
        line_len++;
        br_feed(c);
        HAL_UART_Transmit(&huart2, &c, 1, 10);   /* 에코 */
      }
    }

    if (dropped != 0U)                  /* 링 버퍼 유실 알림 (한 번 알리고 초기화) */
    {
      snprintf(msg, sizeof msg, "\r\n[rx ring buffer dropped %lu byte(s)]\r\n", (unsigned long)dropped);
      dropped = 0U;
      uart_puts(msg);
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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

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
/* HAL 콜백 (ISR 문맥): 1바이트 수신 완료 -> 링 버퍼에 push 후 재무장                */
/* ------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART2) return;
  if (!rb_push(&rb, rx_byte)) dropped++;
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)      /* 오버런(ORE) 등: 수신 재시작 */
{
  if (huart->Instance == USART2) HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

/* ------------------------------------------------------------------------- */
/* SysTick ISR 에서 호출되는 사용자 함수 (ISR 문맥, HAL 콜백 아님)                   */
/*   stm32f1xx_it.c 의 SysTick_Handler -> HAL_IncTick() 다음 USER CODE 영역에서 1ms 마다 호출 */
/*   ISR 은 main 에 끼어들 수 있지만 main 은 ISR 에 끼어들 수 없으므로, ISR 쪽 push 는 보호하지 않는다 */
/* ------------------------------------------------------------------------- */
void Stack_RaceTick(void)
{
  if (!race_active) return;
  if (stk_push(&race_stk, 'I')) race_isr_push++;
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

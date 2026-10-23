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
 * 04_USART_RingBuffer_HAL_c  —  [계층: HAL]  ISR 이 채우고 main 이 비우는 링 버퍼 (생산자-소비자, lock-free)
 *
 * 문제: 수신 인터럽트는 언제 올지 모르는데 main 루프는 다른 일 중일 수 있다. 데이터를 잃지 않으려면 그 사이를 이어 줄 버퍼가 필요하다.
 *
 * 구조
 *   ISR 문맥(HAL_UART_RxCpltCallback) : 수신한 1바이트를 링 버퍼 head 에 push   (생산자)
 *   main 루프                          : tail 에서 pop 하여 처리                    (소비자)
 *   head 는 생산자만, tail 은 소비자만 쓰므로 단일 생산자/단일 소비자에서는 인터럽트를 막지 않아도 안전하다 (lock-free).
 *   버퍼 크기를 2 의 거듭제곱(128)으로 잡아 인덱스 래핑을 "& (SIZE-1)" 마스크 한 번으로 처리한다.
 *   "가득 참" 판정을 위해 한 칸을 비워 둔다: full 이면 (head+1) & mask == tail,  empty 이면 head == tail
 *
 * 시험 방법 (터미널 115200 8N1)
 *   - 평소: 입력한 글자가 에코되고, 엔터를 칠 때마다 통계 "[rx=N, dropped=M, max_used=K]" 출력
 *   - 오버플로 실험: B1(PC13)을 누르면 main 이 2초간 멈춘다(HAL_Delay). 그 동안 터미널에서 긴 텍스트(127바이트 이상)를
 *     붙여 넣으면 버퍼가 가득 차 dropped 가 늘어나는 것을 확인 -> 버퍼 크기/처리 주기 설계의 필요성
 *
 * 하드웨어 대응: USART2->CR1.RXNEIE, SR.RXNE, DR (HAL_UART_Receive_IT 가 관리). NVIC USART2_IRQn.
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define RB_SIZE 128U                      /* 반드시 2 의 거듭제곱 */
#define RB_MASK (RB_SIZE - 1U)

typedef struct {
    volatile uint32_t head;               /* 다음에 쓸 위치 (생산자=ISR 만 수정) */
    volatile uint32_t tail;               /* 다음에 읽을 위치 (소비자=main 만 수정) */
    uint8_t buf[RB_SIZE];
} ringbuf_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static ringbuf_t rb;
static uint8_t rx_byte;
static volatile uint32_t rx_total, dropped, max_used;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t rb_used(const ringbuf_t *r) { return (r->head - r->tail) & RB_MASK; }

/* 생산자 측 (ISR 에서 호출). 가득 차면 새 바이트를 버리고 0 반환 */
static int rb_push(ringbuf_t *r, uint8_t b)
{
  uint32_t next = (r->head + 1U) & RB_MASK;
  if (next == r->tail) return 0;        /* full */
  r->buf[r->head] = b;
  r->head = next;                       /* 데이터를 쓴 "뒤에" head 를 갱신해야 소비자가 미완성 데이터를 읽지 않는다 */
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
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char msg[64];
  uint8_t c;
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
  const char *banner = "\r\n[ring buffer demo] type text, Enter=stats, B1=stall main 2s\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)banner, (uint16_t)strlen(banner), 100);
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* GPIOC->IDR bit13 (B1: 누르면 Low) */
    {
      HAL_UART_Transmit(&huart2, (uint8_t *)"[main stalled 2s]\r\n", 19, 100);
      HAL_Delay(2000);              /* 소비자가 멈춘 동안 생산자(ISR)는 계속 push -> 오버플로 가능 */
      while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {}
    }

    while (rb_pop(&rb, &c))
    {
      if (c == '\r' || c == '\n')
      {
        int n = snprintf(msg, sizeof msg, "\r\n[rx=%lu, dropped=%lu, max_used=%lu/%lu]\r\n",
                         (unsigned long)rx_total, (unsigned long)dropped,
                         (unsigned long)max_used, (unsigned long)(RB_SIZE - 1U));
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
      }
      else
      {
        HAL_UART_Transmit(&huart2, &c, 1, 10);   /* 에코 */
      }
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
  rx_total++;
  if (!rb_push(&rb, rx_byte)) dropped++;
  uint32_t u = rb_used(&rb);
  if (u > max_used) max_used = u;
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)      /* 오버런(ORE) 등: 수신 재시작 */
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

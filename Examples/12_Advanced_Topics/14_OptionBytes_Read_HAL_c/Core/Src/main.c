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
 * 14_OptionBytes_Read_HAL_c  —  [계층: HAL]  옵션 바이트(Option bytes) 읽기: 읽기 보호(RDP), 쓰기 보호(WRP), 사용자 설정
 *
 * 옵션 바이트란 (RM0008 3.3.3 / PM0075 "Option byte description")
 *   Flash 와 별도인 시스템 영역(0x1FFFF800~)에 저장되는 칩 설정. 리셋 시 하드웨어가 읽어 `FLASH->OBR`, `FLASH->WRPR` 에 반영한다.
 *   각 항목은 값과 그 **보수(complement)** 를 함께 저장하고(예: RDP = 0xA5 / nRDP = 0x5A), 불일치하면 오류 상태로 취급한다.
 *
 *   RDP  (Read protection): 0xA5 = **레벨 0**(보호 없음, 디버거/부트로더로 Flash 읽기 가능),  0x00 등 기타 = **레벨 1**(Flash 를 디버거로 읽을 수 없음).
 *        *** 레벨 1 을 해제(레벨 0 으로)하면 **Flash 전체가 자동으로 지워진다**(mass erase). 실수로 켜면 펌웨어를 다시 올릴 수밖에 없다. ***
 *   USER : nWDG_SW (0=하드웨어 워치독: 리셋 직후 IWDG 자동 시작), nRST_STOP (0=Stop 진입 시 리셋), nRST_STDBY (0=Standby 진입 시 리셋)
 *   Data0/Data1 : 사용자 데이터 각 8비트 (설정값 보존용)
 *   WRP0~3 : 쓰기 보호 - 4페이지(4KB) 단위 비트, 0 이면 그 영역이 보호(쓰기/소거 불가). 부트로더/키 영역 보호에 쓴다.
 *
 * 이 예제는 **읽기 전용**이다: 옵션 바이트를 바꾸지 않는다. UART(115200)로 다음을 출력한다.
 *   - HAL_FLASHEx_OBGetConfig() 로 얻은 RDP 레벨, WRP 상태, USER 옵션
 *   - 원시 옵션 바이트 메모리(0x1FFFF800...)와 FLASH->OBR / FLASH->WRPR 레지스터
 *   - 쓰기 보호된 페이지 범위 (기본 소자는 모두 0xFFFF... = 보호 없음)
 *   변경 방법은 STM32CubeProgrammer 의 "OB" 탭, 또는 HAL_FLASHEx_OBProgram() + HAL_FLASH_OB_Launch() 로 코드에서 할 수 있다
 *   (Launch 는 리셋을 일으킨다). 코드로 바꾸는 실험은 반드시 RDP 를 건드리지 않는 항목(예: Data0)으로만 하라.
 *
 * ISR: 사용하지 않는다.
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

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define OB_BASE_ADDR 0x1FFFF800UL        /* 옵션 바이트 시스템 영역 시작 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char m[128];
  FLASH_OBProgramInitTypeDef ob = {0};
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
  HAL_FLASHEx_OBGetConfig(&ob);                                    /* 옵션 바이트를 구조체로 읽기 (RDP, WRP, USER, DATA) */

  print("\r\n=== option bytes (read only) ===\r\n");
  snprintf(m, sizeof m, "RDP level : %s  (HAL value 0x%02lX)\r\n",
           ob.RDPLevel == OB_RDP_LEVEL_0 ? "LEVEL 0 (no read protection)" : "LEVEL 1 (Flash readout protected)", (unsigned long)ob.RDPLevel);
  print(m);
  snprintf(m, sizeof m, "USER      : IWDG=%s, reset on STOP=%s, reset on STANDBY=%s\r\n",
           (ob.USERConfig & OB_IWDG_SW) ? "software (default)" : "HARDWARE (auto start)",
           (ob.USERConfig & OB_STOP_NO_RST) ? "no" : "yes",
           (ob.USERConfig & OB_STDBY_NO_RST) ? "no" : "yes");
  print(m);
  snprintf(m, sizeof m, "WRP pages : 0x%08lX (bit=1: writable, 0: protected in the OBGetConfig bitmap)\r\n", (unsigned long)ob.WRPPage);
  print(m);

  /* 원시 옵션 바이트 메모리: 16비트 항목마다 "값(하위 8비트) + 보수(상위 8비트)" 형태 */
  const volatile uint16_t *raw = (const volatile uint16_t *)OB_BASE_ADDR;
  print("raw @0x1FFFF800: ");
  for (int i = 0; i < 8; i++) { snprintf(m, sizeof m, "%04X ", raw[i]); print(m); }
  print("\r\n  [0]=RDP/nRDP [1]=USER/nUSER [2]=Data0 [3]=Data1 [4..7]=WRP0..WRP3 (각각 값/보수 쌍)\r\n");

  uint32_t obr = FLASH->OBR, wrpr = FLASH->WRPR;
  snprintf(m, sizeof m, "FLASH->OBR = 0x%08lX  (OPTERR=%lu RDPRT=%lu WDG_SW=%lu nRST_STOP=%lu nRST_STDBY=%lu)\r\n",
           (unsigned long)obr, (unsigned long)(obr & 1u), (unsigned long)((obr >> 1) & 1u), (unsigned long)((obr >> 2) & 1u),
           (unsigned long)((obr >> 3) & 1u), (unsigned long)((obr >> 4) & 1u));
  print(m);
  snprintf(m, sizeof m, "FLASH->WRPR = 0x%08lX  (bit=0 -> that 4-page (4KB) block is write-protected)\r\n", (unsigned long)wrpr);
  print(m);

  if (obr & 1u) print("!! OPTERR: option byte complement mismatch (the option bytes are corrupted)\r\n");
  if (wrpr != 0xFFFFFFFFu) print("note: some Flash pages are write-protected (a programmer/bootloader may refuse to erase them)\r\n");
  print("done. (this example does not modify any option byte)\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(1000);
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

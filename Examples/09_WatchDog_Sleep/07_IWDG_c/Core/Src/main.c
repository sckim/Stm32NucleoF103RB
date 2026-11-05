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
 * 07_IWDG_c  —  [계층: HAL]  독립 워치독(IWDG): 저속 내부 클럭(LSI)으로 도는 "마지막 안전장치"
 *
 * 02_WWDG 와의 비교
 *   WWDG : APB1 클럭 기반, "창(window)" 안에서만 리프레시 허용 (너무 빠른 리프레시도 리셋), 조기 경고 인터럽트, 짧은 타임아웃(ms).
 *   IWDG : **독립 저속 오실레이터 LSI(약 40kHz)** 로 동작 -> 메인 클럭이 죽어도(HSE/PLL 장애) 살아 있다. 타임아웃 전에 리프레시하면 되고
 *          (창 없음), 초 단위 타임아웃 가능. 한 번 켜면 **리셋 외에는 끌 수 없다**(LSI 도 자동으로 켜진다).
 *
 * 타임아웃 계산 (RM0008 19장)
 *   t = (reload + 1) x prescaler / f_LSI      (f_LSI 는 30~60kHz 로 개체/온도에 따라 크게 다르다, 공칭 40kHz)
 *   이 예제: prescaler = 64, reload = 624 -> 625 x 64 / 40000 = 1.0 s (공칭).  실제로는 0.67 ~ 1.33 s 범위.
 *   => IWDG 는 "정확한 시간"이 아니라 "이 정도 시간 안에 응답이 없으면 리셋"하는 용도이므로, 리프레시 주기를 타임아웃의 절반 이하(0.5s)로 잡는다.
 *   IWDG->KR 키: 0xCCCC 시작, 0x5555 설정 레지스터 접근 허용, 0xAAAA 리프레시(reload).  PR, RLR, SR(PVU/RVU 갱신 중 표시).
 *
 * 동작
 *   - 부팅 시 리셋 원인 출력 (RCC->CSR.IWDGRSTF -> "IWDG reset"). LD2 점등 1초로 부팅을 알린 뒤 워치독 시작.
 *   - 0.5초마다 HAL_IWDG_Refresh() 하고 LD2 토글 (정상)
 *   - B1(PC13)을 누르면 **프로그램이 멈춘 것을 흉내**(무한 루프, 리프레시 중단) -> 약 1초 뒤 IWDG 가 리셋 -> 부팅 메시지에 "IWDG reset"
 *   - 디버거로 정지 중 워치독이 리셋하지 않도록 __HAL_DBGMCU_FREEZE_IWDG() 로 디버그 중 정지시킨다.
 *
 * 리셋 원인 플래그 (RCC->CSR): IWDGRSTF(bit29), WWDGRSTF(bit30), SFTRSTF(bit28), PORRSTF(bit27), PINRSTF(bit26), LPWRRSTF(bit31)
 *   RMVF(bit24)에 1 을 써서 지운다.
 *
 * ISR: 인터럽트를 사용하지 않는다 (IWDG 는 인터럽트 없이 리셋만 한다).
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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IWDG_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100); }
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  char msg[96];
  uint32_t t_refresh;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  __HAL_DBGMCU_FREEZE_IWDG();                          /* DBGMCU->CR.DBG_IWDG_STOP = 1: 디버거 중단점에서 카운터 정지 (IWDG 시작 전에 설정) */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_IWDG_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 리셋 원인 (RCC->CSR). IWDG 는 CubeMX 구조상 MX_IWDG_Init() 에서 이미 시작됐지만 CSR 플래그에는 영향이 없다 */
  int n = snprintf(msg, sizeof msg, "\r\n[boot] reset cause:%s%s%s%s%s\r\n",
                   __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? " IWDG" : "",
                   __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? " WWDG" : "",
                   __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)  ? " SOFTWARE" : "",
                   __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)  ? " POWER-ON" : "",
                   __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)  ? " NRST-PIN" : "");
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)n, 100);
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) print("*** the previous run was reset by the independent watchdog ***\r\n");
  __HAL_RCC_CLEAR_RESET_FLAGS();                                            /* CSR.RMVF = 1 */

  /* 부팅 표시: LD2 를 1초 켠다. 워치독이 이미 동작 중이므로(타임아웃 ~1s) 250ms 씩 나눠 기다리며 리프레시한다 */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
  for (int i = 0; i < 4; i++)
  {
    HAL_Delay(250);
    HAL_IWDG_Refresh(&hiwdg);                                               /* IWDG->KR = 0xAAAA */
  }
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  print("watchdog started: timeout ~1s (0.67~1.33s by LSI tolerance), refresh every 0.5s, press B1 to simulate a hang\r\n");

  t_refresh = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)   /* GPIOC->IDR bit13 (B1: 누르면 Low) */
    {
      print("B1: simulating a hang (no more refresh) -> reset in ~1s\r\n");
      while (1) {}                                                      /* 멈춘 프로그램: 리프레시 없음 */
    }

    if (HAL_GetTick() - t_refresh >= 500)
    {
      t_refresh += 500;
      HAL_IWDG_Refresh(&hiwdg);                                         /* IWDG->KR = 0xAAAA */
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
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
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Reload = 624;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */
  /* [레지스터] IWDG->KR = 0x5555 (PR/RLR 접근 허용), IWDG->PR = 64 분주 (40kHz/64 = 625Hz), IWDG->RLR = 624 ((624+1)/625Hz = 1.0s),
   * IWDG->KR = 0xCCCC 로 시작 (이후 리셋 외에는 끌 수 없고 LSI 도 자동으로 켜진다). F1 의 IWDG 는 MspInit 이 없다. */
  /* USER CODE END IWDG_Init 2 */

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
  /* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용. 클럭/핀 설정은 stm32f1xx_hal_msp.c 의 HAL_UART_MspInit 에 있다. */
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

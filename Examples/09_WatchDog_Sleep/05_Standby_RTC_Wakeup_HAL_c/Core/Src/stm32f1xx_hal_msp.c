/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32f1xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/

  /** NOJTAG: JTAG-DP Disabled and SW-DP Enabled
  */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/**
  * @brief RTC MSP Initialization
  * This function configures the hardware resources used in this example
  * @param hrtc: RTC handle pointer
  * @retval None
  */
void HAL_RTC_MspInit(RTC_HandleTypeDef* hrtc)
{
  if(hrtc->Instance==RTC)
  {
    /* USER CODE BEGIN RTC_MspInit 0 */
    /* [HAL_RTC_Init 이 호출] RTC 클럭 소스가 아직 없을 때만(=전원 최초 인가) 선택한다.
     * 이미 선택돼 있으면 건드리지 않는다 -> 변경하려면 백업 도메인 리셋이 필요해 BKP/RTC 값이 지워지기 때문.
     * (CubeMX 는 LSE 고정으로 생성하지만, 이 예제는 LSE 실패 시 LSI 로 대체하는 로직을 유지한다.) */
    HAL_PWR_EnableBkUpAccess();                        /* PWR->CR.DBP = 1 : RCC->BDCR 쓰기 허용 */
    if (__HAL_RCC_GET_RTC_SOURCE() == RCC_RTCCLKSOURCE_NO_CLK)   /* RCC->BDCR.RTCSEL == 00 */
    {
      RCC_OscInitTypeDef o = {0};
      RCC_PeriphCLKInitTypeDef pc = {0};

      o.OscillatorType = RCC_OSCILLATORTYPE_LSE;
      o.LSEState = RCC_LSE_ON;                         /* RCC->BDCR.LSEON (시작에 최대 수 초) */
      o.PLL.PLLState = RCC_PLL_NONE;
      if (HAL_RCC_OscConfig(&o) == HAL_OK)
      {
        pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
      }
      else                                             /* LSE 크리스털이 없거나 시작 실패 -> LSI 로 대체 */
      {
        o.OscillatorType = RCC_OSCILLATORTYPE_LSI;
        o.LSEState = RCC_LSE_OFF;
        o.LSIState = RCC_LSI_ON;
        if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();
        pc.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
      }
      pc.PeriphClockSelection = RCC_PERIPHCLK_RTC;
      if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) Error_Handler();   /* RCC->BDCR.RTCSEL */
    }
    /* 아래 생성 코드: BKP 클럭 RCC->APB1ENR.BKPEN, RTC 클럭 RCC->BDCR.RTCEN */
    /* USER CODE END RTC_MspInit 0 */
    HAL_PWR_EnableBkUpAccess();
    /* Enable BKP CLK enable for backup registers */
    __HAL_RCC_BKP_CLK_ENABLE();
    /* Peripheral clock enable */
    __HAL_RCC_RTC_ENABLE();
    /* USER CODE BEGIN RTC_MspInit 1 */

    /* USER CODE END RTC_MspInit 1 */

  }

}

/**
  * @brief RTC MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param hrtc: RTC handle pointer
  * @retval None
  */
void HAL_RTC_MspDeInit(RTC_HandleTypeDef* hrtc)
{
  if(hrtc->Instance==RTC)
  {
    /* USER CODE BEGIN RTC_MspDeInit 0 */

    /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
    /* USER CODE BEGIN RTC_MspDeInit 1 */

    /* USER CODE END RTC_MspDeInit 1 */

  }

}

/**
  * @brief UART MSP Initialization
  * This function configures the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(huart->Instance==USART2)
  {
    /* USER CODE BEGIN USART2_MspInit 0 */

    /* USER CODE END USART2_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = USART_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USART_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USART_RX_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN USART2_MspInit 1 */
    /* [레지스터] USART2 클럭: RCC->APB1ENR.USART2EN, 포트 A 클럭: RCC->APB2ENR.IOPAEN
     * TX(PA2): GPIOA->CRL CNF2=10(AF push-pull), MODE2=11(50MHz) / RX(PA3): CNF3=01(플로팅 입력) */

    /* USER CODE END USART2_MspInit 1 */

  }

}

/**
  * @brief UART MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
  if(huart->Instance==USART2)
  {
    /* USER CODE BEGIN USART2_MspDeInit 0 */

    /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, USART_TX_Pin|USART_RX_Pin);

    /* USER CODE BEGIN USART2_MspDeInit 1 */

    /* USER CODE END USART2_MspDeInit 1 */
  }

}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

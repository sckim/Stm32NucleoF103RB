/**
 * stm32f1xx_it.c — 인터럽트 서비스 루틴(ISR) 모음  [계층: HAL]
 *
 * ISR과 콜백의 역할 구분:
 *   - XXX_IRQHandler (이 파일) : 벡터 테이블이 직접 호출하는 진짜 ISR. HAL_XXX_IRQHandler()로 넘기기만 한다.
 *   - HAL_XXX_Callback (main.c) : HAL이 IRQHandler 안에서 플래그를 해석한 뒤 호출하는 사용자 콜백.
 */
#include "main.h"
#include "stm32f1xx_it.h"
extern TIM_HandleTypeDef htim1;

/* TIM1 브레이크: SR.BIF -> HAL_TIMEx_BreakCallback */
void TIM1_BRK_IRQHandler(void) { HAL_TIM_IRQHandler(&htim1); }

void NMI_Handler(void) { while (1) {} }
void HardFault_Handler(void) { while (1) {} }
void SysTick_Handler(void) { HAL_IncTick(); }   /* 1ms 틱: HAL_Delay/HAL_GetTick 기반 */

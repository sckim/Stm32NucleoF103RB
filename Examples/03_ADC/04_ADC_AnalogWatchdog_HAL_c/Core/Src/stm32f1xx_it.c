/**
 * stm32f1xx_it.c — 인터럽트 서비스 루틴(ISR) 모음  [계층: HAL]
 *
 * ISR과 콜백의 역할 구분:
 *   - XXX_IRQHandler (이 파일) : 벡터 테이블이 직접 호출하는 진짜 ISR. HAL_XXX_IRQHandler()로 넘기기만 한다.
 *   - HAL_XXX_Callback (main.c) : HAL이 IRQHandler 안에서 플래그를 해석한 뒤 호출하는 사용자 콜백.
 */
#include "main.h"
#include "stm32f1xx_it.h"
extern ADC_HandleTypeDef hadc1;

/* ADC1/ADC2 공용 벡터: SR.AWD -> HAL_ADC_LevelOutOfWindowCallback */
void ADC1_2_IRQHandler(void) { HAL_ADC_IRQHandler(&hadc1); }

void NMI_Handler(void) { while (1) {} }
void HardFault_Handler(void) { while (1) {} }
void SysTick_Handler(void) { HAL_IncTick(); }   /* 1ms 틱: HAL_Delay/HAL_GetTick 기반 */

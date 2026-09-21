/**
 * stm32f1xx_it.c — 인터럽트 서비스 루틴(ISR) 모음  [계층: HAL]
 *
 * ISR과 콜백의 역할 구분:
 *   - XXX_IRQHandler (이 파일) : 벡터 테이블이 직접 호출하는 진짜 ISR. HAL_XXX_IRQHandler()로 넘기기만 한다.
 *   - HAL_XXX_Callback (main.c) : HAL이 IRQHandler 안에서 플래그를 해석한 뒤 호출하는 사용자 콜백.
 */
#include "main.h"
#include "stm32f1xx_it.h"
extern I2C_HandleTypeDef hi2c1;
extern DMA_HandleTypeDef hdma_i2c1_rx;
extern DMA_HandleTypeDef hdma_i2c1_tx;

void I2C1_EV_IRQHandler(void) { HAL_I2C_EV_IRQHandler(&hi2c1); }     /* 주소/이벤트 */
void I2C1_ER_IRQHandler(void) { HAL_I2C_ER_IRQHandler(&hi2c1); }     /* 오류(NACK 등) */
void DMA1_Channel6_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_i2c1_tx); }
void DMA1_Channel7_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_i2c1_rx); }   /* TC -> HAL_I2C_MemRxCpltCallback */

void NMI_Handler(void) { while (1) {} }
void HardFault_Handler(void) { while (1) {} }
void SysTick_Handler(void) { HAL_IncTick(); }   /* 1ms 틱: HAL_Delay/HAL_GetTick 기반 */

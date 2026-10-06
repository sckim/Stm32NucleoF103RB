#ifndef __STM32F1xx_IT_H
#define __STM32F1xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* 주차별로 추가되는 IRQ 핸들러 (예시 — 실제 사용하는 것만 CubeMX 생성 목록에 맞춰 추가)
   void EXTI15_10_IRQHandler(void);
   void TIM2_IRQHandler(void);
   void DMA1_Channel1_IRQHandler(void);
   void USART2_IRQHandler(void);
   void I2C1_EV_IRQHandler(void);
   void SPI1_IRQHandler(void);
*/

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_IT_H */

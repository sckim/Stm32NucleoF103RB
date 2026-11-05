/**
  *************** (C) COPYRIGHT 2017 STMicroelectronics ************************
  * @file      startup_stm32f103xb.s
  * @author    MCD Application Team
  * @brief     STM32F103xB Devices vector table for Atollic toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Configure the clock system   
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M3 processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  *            Cortex-M3 리셋 상태 설명:
  *              - Thread mode: 일반 프로그램 코드가 실행되는 모드다. Reset_Handler와
  *                이후 main()은 예외 핸들러가 아니므로 Thread mode에서 실행된다.
  *              - Privileged: 여기서 priority는 NVIC 인터럽트 우선순위가 아니라 권한 수준을
  *                뜻한다. 리셋 후 Thread mode는 privileged 상태이며, 시스템 제어 레지스터와
  *                주변장치에 접근할 수 있다. CONTROL.nPRIV를 설정하면 unprivileged로 낮출 수 있다.
  *              - Main stack: Cortex-M3에는 MSP(Main Stack Pointer)와 PSP(Process Stack Pointer)가
  *                있다. 리셋 시 첫 번째 벡터 항목(_estack)이 MSP에 자동 로드되고, Thread mode는
  *                기본적으로 MSP를 사용한다. CONTROL.SPSEL로 Thread mode의 스택을 PSP로 바꿀 수
  *                있지만, 예외 처리기는 항상 MSP를 사용한다. 따라서 이 시작 코드와 기본 main()은
  *                별도 변경이 없으면 MSP를 스택으로 사용한다.
  */

  .syntax unified   /* ARM 어셈블리 명령어를 통합된 unified 구문으로 사용한다. */
  .cpu cortex-m3    /* Cortex-M3 코어를 대상으로 한다. */
  .fpu softvfp      /* 부동소수점 연산기는 소프트웨어 라이브러리를 사용한다. */
  .thumb            /* Thumb 명령어 집합을 사용한다. */

/* 벡터 테이블과 기본 인터럽트 처리기의 심볼을 링커에 공개한다. */
.global g_pfnVectors
.global Default_Handler

/* 아래 경계 심볼들은 링커 스크립트가 정의한다.
 * _sidata: Flash에 저장된 .data 초기값의 시작 주소다.
 * _sdata~_edata: 실행 시 초기값을 복사해 넣을 RAM의 .data 범위다.
 * _sbss~_ebss: 시작 시 0으로 초기화할 RAM의 .bss 범위다.
 */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

/* STM32F10x 중밀도 제품의 RAM 부팅 모드에서 사용하는 BootROM 주소다. */
.equ  BootRAM, 0xF108F85F
/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:

/* 리셋 후 가장 먼저 실행되는 초기화 함수다.
 * Reset_Handler가 SystemInit()을 호출하며, 호출이 끝나면 다시 여기로 돌아온다.
 * 애플리케이션의 PLL/버스 클럭 설정은 main()의 SystemClock_Config()가 수행한다.
 */
    bl  SystemInit

/* Flash의 .data 초기값을 RAM으로 복사할 주소와 원본 주소를 준비한다.
 * r0 = RAM 목적지 시작, r1 = RAM 목적지 끝, r2 = Flash 원본, r3 = 바이트 오프셋
 */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
/* Flash에서 4바이트를 읽어 RAM에 쓰고 다음 워드로 이동한다. */
  ldr r4, [r2, r3]  /* Flash에서 4바이트 읽기 */
  str r4, [r0, r3]  /* RAM에 4바이트 쓰기 */
  adds r3, r3, #4   /* 다음 워드로 이동 */

LoopCopyDataInit:
/* 현재 RAM 주소가 .data 끝에 도달하기 전까지 초기값 복사를 반복한다. */
  adds r4, r0, r3   /* 현재 RAM 주소 계산 */ 
  cmp r4, r1        /* 현재 RAM 주소가 .data 끝 주소와 같거나 크면 복사 종료 */
  bcc CopyDataInit
  
/* 초기값이 없는 전역/정적 변수 영역인 .bss를 0으로 초기화한다.
 * r2는 현재 RAM 주소, r4는 .bss 끝 주소이며 r3의 0을 각 워드에 씁니다.
 */
  ldr r2, =_sbss    /* .bss 시작 주소를 r2에 로드한다. */
  ldr r4, =_ebss    /* .bss 끝 주소를 r4에 로드한다. */
  movs r3, #0       /* r3에 0을 로드한다. .bss를 0으로 초기화할 때 사용한다. */
  b LoopFillZerobss

FillZerobss:
/* .bss의 현재 워드에 0을 기록하고 다음 워드로 이동한다. */
  str  r3, [r2] /* 현재 RAM 주소에 0을 기록한다. */
  adds r2, r2, #4   /* 다음 워드로 이동한다. */

LoopFillZerobss:
/* .bss 끝까지 0으로 채운다. */
  cmp r2, r4    /* 현재 RAM 주소가 .bss 끝 주소와 같거나 크면 초기화 종료 */
  bcc FillZerobss

/* C/C++ 런타임 초기화를 수행한다(예: 전역 C++ 객체 생성자). */
    bl __libc_init_array
/* 시작 코드 초기화가 끝난 뒤 애플리케이션 진입점인 main()으로 이동한다. */
  bl main
  /* 일반적으로 main()은 반환하지 않는다. 반환하면 호출자 주소로 복귀한다. */
  bx lr
.size Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  /* 별도 처리기가 연결되지 않은 예외/인터럽트는 여기서 멈춰 디버깅할 수 있게 한다. */
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler
/******************************************************************************
* Cortex-M3 예외 및 STM32 주변장치 인터럽트의 벡터 테이블이다.
* 첫 번째 항목은 초기 MSP(Stack Pointer), 두 번째 항목은 리셋 진입 주소다.
* 이후 항목은 예외/IRQ 번호 순서에 맞는 핸들러 주소다.
* 링커 스크립트가 이 섹션을 Flash 시작 주소에 배치하고, 부트 설정에 따라
* Flash가 0x00000000에 별칭(alias)으로 보이므로 리셋 시 이 표를 읽습니다.
******************************************************************************/
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors


g_pfnVectors:

  /* Cortex-M3 코어 예외 벡터: 리셋 시 스택과 첫 실행 주소를 설정한다. */
  .word _estack
  .word Reset_Handler
  /* 코어 예외 핸들러. 예약된 벡터 슬롯은 0으로 둔다. */
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  /* SysTick 타이머 인터럽트. HAL 사용 시 SysTick_Handler가 HAL tick을 갱신한다. */
  .word SysTick_Handler
  /* 아래는 주변장치 IRQ 번호 순서다. 이 순서는 NVIC 벡터 번호와 일치해야 한다. */
  .word WWDG_IRQHandler
  .word PVD_IRQHandler
  .word TAMPER_IRQHandler
  .word RTC_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_IRQHandler
  .word DMA1_Channel3_IRQHandler
  .word DMA1_Channel4_IRQHandler
  .word DMA1_Channel5_IRQHandler
  .word DMA1_Channel6_IRQHandler
  .word DMA1_Channel7_IRQHandler
  .word ADC1_2_IRQHandler
  .word USB_HP_CAN1_TX_IRQHandler
  .word USB_LP_CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SCE_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_IRQHandler
  .word TIM1_UP_IRQHandler
  .word TIM1_TRG_COM_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word USBWakeUp_IRQHandler
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word BootRAM          /* 벡터 테이블 오프셋 0x108: STM32F10x 중밀도 제품의 RAM 부팅용 */

/*******************************************************************************
* 각 예외/IRQ 핸들러의 약한(weak) 기본 심볼을 정의한다.
* 애플리케이션에서 같은 이름의 핸들러를 강한(strong) 심볼로 구현하면
* 링커가 그 구현을 사용하고, 구현이 없으면 Default_Handler로 연결된다.
*
*******************************************************************************/

/* .weak은 대체 가능한 기본 심볼, .thumb_set은 그 심볼을 기본 처리기에 연결한다. */
  .weak NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak MemManage_Handler
  .thumb_set MemManage_Handler,Default_Handler

  .weak BusFault_Handler
  .thumb_set BusFault_Handler,Default_Handler

  .weak UsageFault_Handler
  .thumb_set UsageFault_Handler,Default_Handler

  .weak SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak DebugMon_Handler
  .thumb_set DebugMon_Handler,Default_Handler

  .weak PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

  .weak WWDG_IRQHandler
  .thumb_set WWDG_IRQHandler,Default_Handler

  .weak PVD_IRQHandler
  .thumb_set PVD_IRQHandler,Default_Handler

  .weak TAMPER_IRQHandler
  .thumb_set TAMPER_IRQHandler,Default_Handler

  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Default_Handler

  .weak FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler,Default_Handler

  .weak RCC_IRQHandler
  .thumb_set RCC_IRQHandler,Default_Handler

  .weak EXTI0_IRQHandler
  .thumb_set EXTI0_IRQHandler,Default_Handler

  .weak EXTI1_IRQHandler
  .thumb_set EXTI1_IRQHandler,Default_Handler

  .weak EXTI2_IRQHandler
  .thumb_set EXTI2_IRQHandler,Default_Handler

  .weak EXTI3_IRQHandler
  .thumb_set EXTI3_IRQHandler,Default_Handler

  .weak EXTI4_IRQHandler
  .thumb_set EXTI4_IRQHandler,Default_Handler

  .weak DMA1_Channel1_IRQHandler
  .thumb_set DMA1_Channel1_IRQHandler,Default_Handler

  .weak DMA1_Channel2_IRQHandler
  .thumb_set DMA1_Channel2_IRQHandler,Default_Handler

  .weak DMA1_Channel3_IRQHandler
  .thumb_set DMA1_Channel3_IRQHandler,Default_Handler

  .weak DMA1_Channel4_IRQHandler
  .thumb_set DMA1_Channel4_IRQHandler,Default_Handler

  .weak DMA1_Channel5_IRQHandler
  .thumb_set DMA1_Channel5_IRQHandler,Default_Handler

  .weak DMA1_Channel6_IRQHandler
  .thumb_set DMA1_Channel6_IRQHandler,Default_Handler

  .weak DMA1_Channel7_IRQHandler
  .thumb_set DMA1_Channel7_IRQHandler,Default_Handler

  .weak ADC1_2_IRQHandler
  .thumb_set ADC1_2_IRQHandler,Default_Handler

  .weak USB_HP_CAN1_TX_IRQHandler
  .thumb_set USB_HP_CAN1_TX_IRQHandler,Default_Handler

  .weak USB_LP_CAN1_RX0_IRQHandler
  .thumb_set USB_LP_CAN1_RX0_IRQHandler,Default_Handler

  .weak CAN1_RX1_IRQHandler
  .thumb_set CAN1_RX1_IRQHandler,Default_Handler

  .weak CAN1_SCE_IRQHandler
  .thumb_set CAN1_SCE_IRQHandler,Default_Handler

  .weak EXTI9_5_IRQHandler
  .thumb_set EXTI9_5_IRQHandler,Default_Handler

  .weak TIM1_BRK_IRQHandler
  .thumb_set TIM1_BRK_IRQHandler,Default_Handler

  .weak TIM1_UP_IRQHandler
  .thumb_set TIM1_UP_IRQHandler,Default_Handler

  .weak TIM1_TRG_COM_IRQHandler
  .thumb_set TIM1_TRG_COM_IRQHandler,Default_Handler

  .weak TIM1_CC_IRQHandler
  .thumb_set TIM1_CC_IRQHandler,Default_Handler

  .weak TIM2_IRQHandler
  .thumb_set TIM2_IRQHandler,Default_Handler

  .weak TIM3_IRQHandler
  .thumb_set TIM3_IRQHandler,Default_Handler

  .weak TIM4_IRQHandler
  .thumb_set TIM4_IRQHandler,Default_Handler

  .weak I2C1_EV_IRQHandler
  .thumb_set I2C1_EV_IRQHandler,Default_Handler

  .weak I2C1_ER_IRQHandler
  .thumb_set I2C1_ER_IRQHandler,Default_Handler

  .weak I2C2_EV_IRQHandler
  .thumb_set I2C2_EV_IRQHandler,Default_Handler

  .weak I2C2_ER_IRQHandler
  .thumb_set I2C2_ER_IRQHandler,Default_Handler

  .weak SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler

  .weak SPI2_IRQHandler
  .thumb_set SPI2_IRQHandler,Default_Handler

  .weak USART1_IRQHandler
  .thumb_set USART1_IRQHandler,Default_Handler

  .weak USART2_IRQHandler
  .thumb_set USART2_IRQHandler,Default_Handler

  .weak USART3_IRQHandler
  .thumb_set USART3_IRQHandler,Default_Handler

  .weak EXTI15_10_IRQHandler
  .thumb_set EXTI15_10_IRQHandler,Default_Handler

  .weak RTC_Alarm_IRQHandler
  .thumb_set RTC_Alarm_IRQHandler,Default_Handler

  .weak USBWakeUp_IRQHandler
  .thumb_set USBWakeUp_IRQHandler,Default_Handler

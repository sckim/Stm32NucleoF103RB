/**
 * 12_ITM_SWO_Printf_Reg_c  —  [계층: 레지스터(CMSIS)]  ITM/SWO 로 printf: UART 핀 없이 디버거 연결만으로 출력
 *
 * SWO(Serial Wire Output)란
 *   Cortex-M3 의 ITM(Instrumentation Trace Macrocell)이 내보내는 텍스트/이벤트를 TPIU 가 직렬화해 SWO 핀(F1 은 PB3)으로 내보낸다.
 *   ST-LINK 가 이를 받아 PC 의 디버그 도구(STM32CubeIDE "SWV ITM Data Console", STM32CubeProgrammer, OpenOCD 등)로 전달한다.
 *   장점: UART 핀·배선 불필요, 매우 빠름(약 2MHz), 인터럽트 안에서도 짧게 쓸 수 있음(블로킹 최소).
 *   단점: 디버거 연결 필요(디버거를 떼면 출력이 버려짐). 코어 클럭을 알려 줘야 SWO 속도를 맞출 수 있다.
 *
 * 필요한 것
 *   - Nucleo-F103RB 에서 PB3(TRACESWO)가 ST-LINK 의 SWO 에 연결돼 있어야 한다. UM1724 의 솔더 브리지 표에서 "ST-LINK SWO" 를 확인
 *     (개정에 따라 기본 연결이 다를 수 있음). PB3 를 GPIO 로 쓰고 있으면(AFIO->MAPR SWJ_CFG 로 JTAG 해제 등) 충돌하니 이 예제에서는 건드리지 않는다.
 *   - 디버그 도구의 "Core clock" 을 8 MHz(HSI 기본)로, SWO 속도는 2 MHz(도구 기본값 사용)로 설정.
 *
 * 코드 요점
 *   1) CoreDebug->DEMCR.TRCENA = 1   : 트레이스 블록 클럭 허용
 *   2) DBGMCU->CR.TRACE_IOEN = 1, TRACE_MODE = 00 : 비동기(SWO) 트레이스 핀 활성 (RM0008 31장 DBG)
 *   3) TPI->ACPR = f_core/f_swo - 1 (SWO 비트 속도 분주), TPI->SPPR = 2 (NRZ/UART 형식), TPI->FFCR = 0x100 (포매터 끔: 순수 ITM 바이트 스트림)
 *   4) ITM->LAR = 0xC5ACCE55 (잠금 해제), ITM->TCR.ITMENA/SWOENA, ITM->TER = 1 (자극 포트 0 허용)
 *   5) printf 의 최종 출력 함수 _write() 를 재정의해 ITM_SendChar()(포트 0)로 보낸다 (newlib 의 stdout 리타깃)
 *   * 디버거가 이미 이 설정을 해 주는 경우가 많으므로, 코드의 설정은 "디버거 없이 켜졌을 때"를 위한 최소 구성이다.
 *
 * 동작: 500ms 마다 "tick N, VTOR=..., SP=..." 를 printf 로 출력 (SWO 콘솔에서 확인). LD2 는 함께 점멸.
 *
 * ISR: SysTick_Handler (아래). 콜백 개념은 없다.
 */
#include "stm32f1xx.h"
#include <stdint.h>
#include <stdio.h>

#define SWO_BAUD 2000000UL

static volatile uint32_t g_ms;
void SysTick_Handler(void) { g_ms++; }

static void swo_init(uint32_t core_hz)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;                   /* 1) 트레이스 활성 */
    DBGMCU->CR = (DBGMCU->CR & ~DBGMCU_CR_TRACE_MODE) | DBGMCU_CR_TRACE_IOEN;   /* 2) TRACE_MODE=00(비동기), 핀 활성 */
    TPI->ACPR = core_hz / SWO_BAUD - 1u;                              /* 3) SWO 클럭 분주 */
    TPI->SPPR = 2u;                                                   /*    NRZ 인코딩 */
    TPI->FFCR = 0x100u;                                               /*    포매터 끄기(EnFCont=0, TrigIn=1) */
    ITM->LAR = 0xC5ACCE55u;                                           /* 4) ITM 잠금 해제 */
    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_SWOENA_Msk | (1u << ITM_TCR_TraceBusID_Pos);
    ITM->TER = 1u;                                                    /*    자극 포트 0 허용 */
}

/* newlib printf/puts 가 호출하는 저수준 출력. 표준출력(1)과 표준오류(2)를 SWO 로 보낸다 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++) ITM_SendChar((uint32_t)ptr[i]);     /* 포트 0 에 1바이트 (내부에서 FIFO 준비 대기) */
    return len;
}

int main(void)
{
    swo_init(SystemCoreClock);                                        /* 리셋 후 HSI 8MHz */

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 20)) | (0x2u << 20);         /* LD2 */

    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    printf("\n[ITM/SWO printf] core=%lu Hz, SWO=%lu baud\n", (unsigned long)SystemCoreClock, (unsigned long)SWO_BAUD);

    uint32_t t_print = 0, n = 0;
    while (1)
    {
        if ((uint32_t)(g_ms - t_print) >= 500u)
        {
            t_print += 500u;
            GPIOA->ODR ^= (1u << 5);
            printf("tick %lu  VTOR=0x%08lX  SP=0x%08lX\n", (unsigned long)n++, (unsigned long)SCB->VTOR, (unsigned long)__get_MSP());
        }
    }
}

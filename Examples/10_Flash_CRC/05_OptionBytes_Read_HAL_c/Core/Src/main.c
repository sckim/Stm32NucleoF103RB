/**
 * 05_OptionBytes_Read_HAL_c  —  [계층: HAL]  옵션 바이트(Option bytes) 읽기: 읽기 보호(RDP), 쓰기 보호(WRP), 사용자 설정
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
#include "main.h"
#include <stdio.h>
#include <string.h>

#define OB_BASE_ADDR 0x1FFFF800UL

UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_USART2_UART_Init(void);

static void print(const char *s) { HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    char m[128];
    FLASH_OBProgramInitTypeDef ob = {0};
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

    while (1) { HAL_Delay(1000); }
}

/* USART2 (ST-Link 가상 COM, PA2=TX / PA3=RX) 기본 폴링 송신용 MSP: 클럭/핀만 설정 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();   /* RCC->APB1ENR.USART2EN */
    __HAL_RCC_GPIOA_CLK_ENABLE();    /* RCC->APB2ENR.IOPAEN   */
    g.Pin = GPIO_PIN_2;  g.Mode = GPIO_MODE_AF_PP;  g.Speed = GPIO_SPEED_FREQ_HIGH;   /* TX: CNF=10, MODE=11 */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;  g.Mode = GPIO_MODE_INPUT;  g.Pull = GPIO_NOPULL;            /* RX: CNF=01 (플로팅 입력) */
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* HSI(8MHz)/2 x16 = 64MHz. Nucleo 는 HSE 가 ST-Link 에서 오므로 HSI 로 단순화 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    o.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();

    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2;          /* APB1 = 32MHz (최대 36MHz) */
    c.APB2CLKDivider = RCC_HCLK_DIV1;          /* APB2 = 64MHz */
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

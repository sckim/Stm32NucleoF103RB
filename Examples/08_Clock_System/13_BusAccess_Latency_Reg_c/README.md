# 13_BusAccess_Latency_Reg_c

> 계층: **레지스터(CMSIS)**, HAL 미사용 — 검토 전 (PlatformIO 빌드만 확인)

교안 「메모리 맵과 버스(AHB/APB)」 4절(버스 클럭과 접근 시간)과 6절(접근 폭·비트 밴딩)의 실습이다.
같은 `LDR`/`STR` 명령이라도 **어느 버스에 붙은 주소냐**에 따라 걸리는 사이클이 다르다는 것을 DWT로 실측하고, APB 브리지가 8·16비트 쓰기를 32비트로 바꾸는 동작을 확인한다.

## 핵심 학습 내용

- **버스 경로**(RM0008 3.1): 코어 System 버스 → 버스 행렬 → SRAM / FLITF(Flash) / AHB→APB 브리지 2개. 이 설정에서 PCLK2 = 64 MHz, PCLK1 = 32 MHz이므로 APB1 한 사이클은 HCLK 두 사이클이다.
- **Flash 데이터 읽기**: SYSCLK 64 MHz에서는 `FLASH_ACR.LATENCY = 2`(RM0008 3.3.3). 프리페치 버퍼는 ICode 명령어 인출용이라 데이터 읽기에는 쓰이지 않는다.
- **쓰기 버퍼**: 쓰기는 코어가 버퍼에 넣고 먼저 진행할 수 있어(posted write) 같은 주소의 읽기보다 짧게 보일 수 있다.
- **측정 방법**: 대상마다 volatile 접근 8회·16회를 펼친 루프를 256번씩 돌리고 `(t16 − t8) / (8 × 256)`으로 접근 1회 비용을 구한다. 루프 오버헤드가 상쇄된다.
- **APB 8·16비트 → 32비트 복제**(RM0008 3.1 Note): 16비트로 `GPIOB->ODR+2`에 0x00A5를 쓰면 하위 하프워드에도 0x00A5가 들어가는지, 8비트로 `TIM2->CCR1+1`에 0x12를 쓰면 0x1212가 되는지 확인한다. GPIO는 32비트 접근만, 타이머는 하프워드·워드 쓰기만 허용되므로 이 실험은 **규칙을 어겼을 때 실제로 생기는 일**을 보는 것이다.
- **비트 밴딩 별칭**: `0x42000000 + offset × 32 + bit × 4`. `GPIOA_ODR` bit5 → `0x42210194`, `RCC_APB2ENR` bit2 → `0x42420308`.
- **함정 하나**: `static const volatile` 변수는 GCC가 `.data`(SRAM)에 둔다. Flash 읽기를 재려면 `const` 변수를 volatile 포인터로 읽는다(소스 주석 참고). 부팅 출력의 주소(0x0800…, 0x2000…)로 확인한다.

## 동작

배선 없음. USART2(ST-LINK 가상 COM, 115200 8N1)로 약 2초마다 출력한다.

```
13_BusAccess_Latency_Reg_c  SYSCLK=HCLK=PCLK2=64 MHz, PCLK1=32 MHz, Flash LATENCY=2
  &g_sram_word = 0x20000000  &g_flash_word = 0x08000944
[round 1]
A) cycles per access (HCLK)
  read  SRAM  variable        : x.xx cycles
  read  Flash const (LAT=2)   : x.xx cycles
  read  APB2  GPIOA->IDR      : x.xx cycles
  read  APB1  TIM2->CNT       : x.xx cycles
  write SRAM  variable        : x.xx cycles
  write APB2  GPIOB->BSRR     : x.xx cycles
  write APB1  TIM2->CCR1      : x.xx cycles
B) APB bridge 8/16-bit -> 32-bit duplication
  GPIOB->ODR after 16-bit write 0x00A5 to (ODR+2): 0x........   (hypothesis: 0x000000A5)
  TIM2->CCR1 after 8-bit write 0x12 to (CCR1+1)  : 0x........   (hypothesis: 0x00001212)
C) bit-band alias
  alias(GPIOA_ODR bit5)   = 0x42210194  read 1  ODR=0x00000020
  alias(RCC_APB2ENR bit2) = 0x42420308  read 1  (expect 1)
```

(A·B의 값은 실측으로 채운다. 기대: SRAM < APB2 < APB1 순으로 길어지고, Flash 읽기는 웨이트 스테이트만큼 SRAM보다 길다.)

## 생각해 볼 것

- `FLASH_ACR.LATENCY`를 바꾸지 않고 SYSCLK만 64 MHz로 올리면 무슨 일이 생기는가? (클럭 트리 장)
- APB1 레지스터를 루프 안에서 자주 읽는 코드(예: 타이머 CNT 폴링)는 어떤 비용을 치르는가?
- 비트 밴딩 쓰기도 버스에서는 "읽고-수정하고-쓰기"이다. `TIMx_SR`처럼 0을 써서 지우는(rc_w0) 레지스터에 쓰면 어떤 위험이 있는가?

## ISR

사용하지 않는다.

## 관련 예제

`02_MemoryMap_Reg_c`(주소 공간과 memory-mapped I/O), `10_DWT_Profiling_Reg_c`(DWT 사이클 측정), `01_GPIO/09_BitBandingReg`(비트 밴딩), `04_Clock_Config_Reg_c`(Flash 웨이트와 PLL 전환).

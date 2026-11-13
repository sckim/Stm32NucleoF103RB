# all_pins.tcl - DE1-SoC(Cyclone V 5CSEMA5F31C6) 핀 배치 (스프레드시트 'DE1-SoC Pin assign'에 있는 핀만)
#
# 출처: 스프레드시트 'DE1-SoC Pin assign'의 표 3-5(CLK), 3-6(SW), 3-7(KEY), 3-8(LEDR), 3-9(7SEG),
#   3-11(GPIO 2x36), 3-16(VGA). 시트의 값을 공개된 Terasic DE1_SoC.qsf 두 개와 대조해 일치함을 확인했다.
#   (시트의 '5CSEMA5' 탭은 칩 핀 목록이어서 이 파일에 넣지 않았다.)
# 시트에 없는 핀(SDRAM, 오디오, ADC, USB, PS/2, TV 디코더, 팬, I2C, 적외선)은 지정하지 않는다.
#
# 사용: Quartus 프로젝트를 연 상태에서 Tcl Console 에서  source all_pins.tcl
#   설계 HDL에 없는 포트의 줄은 "assignment ... ignored" 경고만 내고 무시된다. 오류가 아니다.
#   HDL 포트 이름은 아래 이름(대소문자 포함)과 같아야 하고, 버스 범위도 맞아야 한다.
#   예) output [9:0] LEDR,  input [3:0] KEY,  inout [35:0] GPIO_0

# 이름[번호] 형태의 버스 핀을 한 줄로 배치하는 보조 절차
proc pins {name list} {
    set i 0
    foreach p $list {
        set_location_assignment PIN_$p -to "${name}\[$i\]"
        incr i
    }
}

# ---- 클럭 (표 3-5, 3.3 V) ----
set_location_assignment PIN_AF14 -to CLOCK_50
set_location_assignment PIN_AA16 -to CLOCK2_50
set_location_assignment PIN_Y26 -to CLOCK3_50
set_location_assignment PIN_K14 -to CLOCK4_50
# HPS(ARM 쪽) 25 MHz 클럭 입력. 시트 표 3-5에 있어서 넣었다.
set_location_assignment PIN_D25 -to HPS_CLOCK1_25
set_location_assignment PIN_F25 -to HPS_CLOCK2_25

# ---- 슬라이드 스위치 (표 3-6, 위 = High, 아래 = Low) ----
pins SW      {AB12 AC12 AF9 AF10 AD11 AD12 AE11 AC9 AD10 AE12}

# ---- 푸시버튼 (표 3-7, 풀업이므로 누르면 0) ----
pins KEY     {AA14 AA15 W15 Y16}

# ---- LED (표 3-8, Active High: 1일 때 켜짐) ----
pins LEDR    {V16 W16 V17 V18 W17 W19 Y19 W20 W21 Y21}

# ---- 7세그먼트 (표 3-9, Active Low·공통 애노드: 0일 때 켜짐, [0]=a ... [6]=g) ----
pins HEX0    {AE26 AE27 AE28 AG27 AF28 AG28 AH28}
pins HEX1    {AJ29 AH29 AH30 AG30 AF29 AF30 AD27}
pins HEX2    {AB23 AE29 AD29 AC28 AD30 AC29 AC30}
pins HEX3    {AD26 AC27 AD25 AC25 AB28 AB25 AB22}
pins HEX4    {AA24 Y23 Y24 W22 W24 V23 W25}
pins HEX5    {V25 AA28 Y27 AB27 AB26 AA26 AA25}

# ---- 확장 헤더 JP1(GPIO_0)과 JP2(GPIO_1) (표 3-11) ----
pins GPIO_0  {AC18 Y17 AD17 Y18 AK16 AK18 AK19 AJ19 AJ17 AJ16 AH18 AH17 AG16 AE16 AF16 AG17 AA18 AA19 AE17 AC20 AH19 AJ20 AH20 AK21 AD19 AD20 AE18 AE19 AF20 AF21 AF19 AG21 AF18 AG20 AG18 AJ21}
pins GPIO_1  {AB17 AA21 AB21 AC23 AD24 AE23 AE24 AF25 AF26 AG25 AG26 AH24 AH27 AJ27 AK29 AK28 AK27 AJ26 AK26 AH25 AJ25 AJ24 AK24 AG23 AK23 AH23 AK22 AJ22 AH22 AG22 AF24 AF23 AE22 AD21 AA20 AC22}

# ---- VGA (표 3-16) ----
pins VGA_R   {A13 C13 E13 B12 C12 D12 E12 F13}
pins VGA_G   {J9 J10 H12 G10 G11 G12 F11 E11}
pins VGA_B   {B13 G13 H13 F14 H14 F15 G15 J14}
set_location_assignment PIN_A11 -to VGA_CLK
set_location_assignment PIN_F10 -to VGA_BLANK_N
set_location_assignment PIN_B11 -to VGA_HS
set_location_assignment PIN_D11 -to VGA_VS
set_location_assignment PIN_C10 -to VGA_SYNC_N

# 모든 핀의 I/O 표준을 3.3 V로 둔다 (시트의 모든 핀이 3.3 V이다)
set_global_assignment -name STRATIX_DEVICE_IO_STANDARD "3.3-V LVTTL"
export_assignments

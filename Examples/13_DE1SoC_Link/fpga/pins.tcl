# pins.tcl - DE1-SoC 핀 배치 (실습 1~4 공통)
# 사용: Quartus 프로젝트 폴더에 복사한 뒤 Tcl Console 에서  source pins.tcl

# 이름[번호] 형태의 버스 핀을 한 줄로 배치하는 보조 절차
proc pins {name list} {
    set i 0
    foreach p $list {
        set_location_assignment PIN_$p -to "${name}\[$i\]"
        incr i
    }
}

# 클럭과 MCU 연결 핀
set_location_assignment PIN_AF14 -to CLOCK_50
set_location_assignment PIN_AC18 -to MCU_TX
set_location_assignment PIN_Y17  -to MCU_RX
set_location_assignment PIN_AD17 -to MCU_PWM
set_location_assignment PIN_AC18 -to MCU_CLK     ;# 실습 4: MCU_TX와 같은 핀(JP1 1번)을 쓴다

# 선이 빠져도 입력이 뜨지 않게 약한 풀업을 켠다
set_instance_assignment -name WEAK_PULL_UP_RESISTOR ON -to MCU_TX
set_instance_assignment -name WEAK_PULL_UP_RESISTOR ON -to MCU_PWM
set_instance_assignment -name WEAK_PULL_UP_RESISTOR ON -to MCU_CLK

# 스위치, LED, 7세그먼트
set_location_assignment PIN_AB12 -to "SW\[0\]"
pins LEDR {V16 W16 V17 V18 W17 W19 Y19 W20 W21 Y21}
pins HEX0 {AE26 AE27 AE28 AG27 AF28 AG28 AH28}
pins HEX1 {AJ29 AH29 AH30 AG30 AF29 AF30 AD27}
pins HEX2 {AB23 AE29 AD29 AC28 AD30 AC29 AC30}
pins HEX3 {AD26 AC27 AD25 AC25 AB28 AB25 AB22}
pins HEX4 {AA24 Y23 Y24 W22 W24 V23 W25}
pins HEX5 {V25 AA28 Y27 AB27 AB26 AA26 AA25}

# 모든 핀의 기본 I/O 표준을 3.3 V로 둔다
set_global_assignment -name STRATIX_DEVICE_IO_STANDARD "3.3-V LVTTL"
export_assignments

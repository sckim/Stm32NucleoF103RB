// lab1_top.v
// 실습 1: MCU가 보낸 UART 한 프레임을 LEDR9~LEDR0에 비트 그대로 보여 준다
module lab1_top (
    input  wire       CLOCK_50,
    input  wire       MCU_TX,     // GPIO_0[0] <- Nucleo D8 (PA9, USART1_TX)
    output wire [9:0] LEDR        // LEDR9 = 시작, LEDR8~1 = D0~D7, LEDR0 = 정지
);
    wire rx_s;                    // 동기화된 수신선 (SignalTap에서 볼 신호)
    wire busy;                    // 프레임 수신 중 (SignalTap에서 볼 신호)

    sync2      u_sync (.clk(CLOCK_50), .d(MCU_TX), .q(rx_s));
    frame_leds u_cap  (.clk(CLOCK_50), .rx(rx_s), .frame(LEDR), .busy(busy));
endmodule

// lab2_top.v
// 실습 2: 받은 바이트를 HEX1~HEX0에 표시하고, 1을 더해 MCU로 돌려보낸다
module lab2_top #(parameter CLKS_PER_BIT = 434) (
    input  wire       CLOCK_50,
    input  wire       MCU_TX,     // GPIO_0[0] <- Nucleo D8 (PA9,  USART1_TX)
    output wire       MCU_RX,     // GPIO_0[1] -> Nucleo D2 (PA10, USART1_RX)
    output wire [6:0] HEX0, HEX1,
    output wire [9:0] LEDR
);
    wire       rx_s, valid, busy;
    wire [7:0] rx_data;
    reg  [7:0] shown = 8'd0;      // 화면에 유지할 마지막 수신 바이트
    reg        blink = 1'b0;      // 바이트를 받을 때마다 반전

    sync2 u_sync (.clk(CLOCK_50), .d(MCU_TX), .q(rx_s));

    uart_rx #(.CLKS_PER_BIT(CLKS_PER_BIT)) u_rx
        (.clk(CLOCK_50), .rx(rx_s), .data(rx_data), .valid(valid));

    uart_tx #(.CLKS_PER_BIT(CLKS_PER_BIT)) u_tx
        (.clk(CLOCK_50), .start(valid), .data(rx_data + 8'd1), .tx(MCU_RX), .busy(busy));

    always @(posedge CLOCK_50)
        if (valid) begin
            shown <= rx_data;
            blink <= ~blink;
        end

    hex7seg u_h0 (.x(shown[3:0]), .seg(HEX0));
    hex7seg u_h1 (.x(shown[7:4]), .seg(HEX1));
    assign LEDR = {blink, busy, shown};   // LEDR9: 수신마다 반전, LEDR7~0: 수신 바이트
endmodule

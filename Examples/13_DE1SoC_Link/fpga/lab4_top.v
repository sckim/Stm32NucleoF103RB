// lab4_top.v
// 실습 4: MCU가 만든 클럭(1 Hz)의 상승 엣지를 세어 카운터 값을 표시한다
//   LEDR0~LEDR9 : 카운터 하위 10비트 (2진수, LEDR0 = 최하위 비트)
//   HEX5~HEX0   : 카운터 24비트 전체 (16진수 6자리, HEX0 = 최하위 자리)
//   SW0 = 1     : 카운터를 0으로 되돌린다 (비동기 입력이므로 동기화기를 거친다)
module lab4_top (
    input  wire       CLOCK_50,
    input  wire       MCU_CLK,    // GPIO_0[0] <- Nucleo D8 (PA9, TIM1_CH2)
    input  wire [0:0] SW,
    output wire [6:0] HEX0, HEX1, HEX2, HEX3, HEX4, HEX5,
    output wire [9:0] LEDR
);
    // 1) MCU 클럭은 CLOCK_50과 무관한 비동기 신호이므로 2단 동기화기로 옮긴다.
    //    MCU 클럭을 FPGA의 클럭으로 직접 쓰지 않고, 동기화한 뒤 엣지만 검출한다.
    wire clk_s, rst_s;
    reg  clk_d = 1'b0;
    sync2 #(.INIT(1'b0)) u_sync_clk (.clk(CLOCK_50), .d(MCU_CLK), .q(clk_s));
    sync2 #(.INIT(1'b0)) u_sync_rst (.clk(CLOCK_50), .d(SW[0]),   .q(rst_s));
    always @(posedge CLOCK_50) clk_d <= clk_s;
    wire rise = clk_s & ~clk_d;           // MCU 클럭의 상승 엣지: CLOCK_50 한 주기 동안만 1

    // 2) 24비트 카운터: 상승 엣지마다 1 증가
    reg [23:0] cnt = 24'd0;
    always @(posedge CLOCK_50)
        if (rst_s)     cnt <= 24'd0;
        else if (rise) cnt <= cnt + 24'd1;

    // 3) 표시
    assign LEDR = cnt[9:0];
    hex7seg u0 (.x(cnt[ 3: 0]), .seg(HEX0));
    hex7seg u1 (.x(cnt[ 7: 4]), .seg(HEX1));
    hex7seg u2 (.x(cnt[11: 8]), .seg(HEX2));
    hex7seg u3 (.x(cnt[15:12]), .seg(HEX3));
    hex7seg u4 (.x(cnt[19:16]), .seg(HEX4));
    hex7seg u5 (.x(cnt[23:20]), .seg(HEX5));
endmodule

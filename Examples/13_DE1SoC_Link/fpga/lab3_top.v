// lab3_top.v
// 실습 3: PWM의 주기와 High 시간을 0.1 us 단위로 재어 HEX5~HEX0에 10진수로 표시한다
//   SW0 = 0 : 주기,  SW0 = 1 : High 시간   (표시값 010000 = 1000.0 us)
module lab3_top #(parameter CLKS_PER_TICK = 5) (   // 50 MHz / 5 = 10 MHz -> 0.1 us
    input  wire       CLOCK_50,
    input  wire       MCU_PWM,    // GPIO_0[2] <- Nucleo D7 (PA8, TIM1_CH1)
    input  wire [0:0] SW,
    output wire [6:0] HEX0, HEX1, HEX2, HEX3, HEX4, HEX5,
    output wire [9:0] LEDR
);
    // 1) 동기화 + 엣지 검출
    wire pwm_s;
    reg  pwm_d = 1'b0;
    sync2 #(.INIT(1'b0)) u_sync (.clk(CLOCK_50), .d(MCU_PWM), .q(pwm_s));
    always @(posedge CLOCK_50) pwm_d <= pwm_s;
    wire rise =  pwm_s & ~pwm_d;
    wire fall = ~pwm_s &  pwm_d;

    // 2) 0.1 us 틱 발생기
    reg [2:0] pre = 3'd0;
    wire tick = (pre == CLKS_PER_TICK - 1);
    always @(posedge CLOCK_50) pre <= tick ? 3'd0 : pre + 3'd1;

    // 3) 6자리 BCD 카운터 (자리마다 0~9, 9 다음에 윗자리로 올림)
    reg [23:0] cnt = 24'd0;       // {10^5, 10^4, 10^3, 10^2, 10^1, 10^0}
    reg [23:0] nxt;
    reg        carry;
    integer    i;
    always @(*) begin
        nxt   = cnt;
        carry = 1'b1;                              // 맨 아랫자리에 1을 더한다
        for (i = 0; i < 6; i = i + 1)
            if (carry) begin
                if (cnt[4*i +: 4] == 4'd9) nxt[4*i +: 4] = 4'd0;      // 올림 유지
                else begin nxt[4*i +: 4] = cnt[4*i +: 4] + 4'd1; carry = 1'b0; end
            end
    end

    // 4) 상승 엣지에서 주기를, 하강 엣지에서 High 시간을 붙잡는다
    reg [23:0] period = 24'd0, high = 24'd0;
    always @(posedge CLOCK_50) begin
        if (fall) high <= cnt;
        if (rise) begin
            period <= cnt;
            cnt    <= 24'd0;                       // 다음 주기 측정 시작
        end else if (tick) cnt <= nxt;
    end

    // 5) 표시
    wire [23:0] show = SW[0] ? high : period;
    hex7seg u0 (.x(show[ 3: 0]), .seg(HEX0));
    hex7seg u1 (.x(show[ 7: 4]), .seg(HEX1));
    hex7seg u2 (.x(show[11: 8]), .seg(HEX2));
    hex7seg u3 (.x(show[15:12]), .seg(HEX3));
    hex7seg u4 (.x(show[19:16]), .seg(HEX4));
    hex7seg u5 (.x(show[23:20]), .seg(HEX5));
    assign LEDR = {9'd0, pwm_s};                   // LEDR0: PWM 밝기로 듀티를 눈으로 본다
endmodule

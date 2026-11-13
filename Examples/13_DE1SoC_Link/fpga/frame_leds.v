// frame_leds.v
// 시작 비트의 하강 엣지부터 1비트 간격으로 10번 표본화해 한 프레임을 LED에 보여 준다.
//   frame[9] = 시작 비트, frame[8:1] = D0~D7 (도착한 순서), frame[0] = 정지 비트
module frame_leds #(parameter CLKS_PER_BIT = 434) (   // 50 MHz / 115200 bps = 434
    input  wire       clk,
    input  wire       rx,                 // 동기화된 수신선
    output reg  [9:0] frame = 10'h3FF,    // 마지막으로 받은 프레임 (처음에는 모두 1)
    output reg        busy  = 1'b0        // 프레임을 받는 동안 1
);
    reg        rx_d = 1'b1;               // 한 클럭 전의 rx (엣지 검출용)
    reg [15:0] cnt  = 16'd0;              // 다음 표본화까지 남은 클럭 수
    reg [3:0]  n    = 4'd0;               // 지금까지 읽은 비트 수
    reg [9:0]  sh   = 10'h3FF;            // 시프트 레지스터

    always @(posedge clk) begin
        rx_d <= rx;
        if (!busy) begin
            if (rx_d & ~rx) begin         // 하강 엣지 = 시작 비트의 시작
                busy <= 1'b1;
                cnt  <= CLKS_PER_BIT/2;   // 반 비트 뒤 = 시작 비트의 가운데
                n    <= 4'd0;
            end
        end else if (cnt == 16'd0) begin  // 비트의 가운데에 왔다
            sh  <= {sh[8:0], rx};         // 왼쪽으로 밀며 새 비트를 오른쪽에 넣는다
            cnt <= CLKS_PER_BIT - 1;
            n   <= n + 4'd1;
            if (n == 4'd9) begin          // 10번째(정지 비트)까지 읽었다
                frame <= {sh[8:0], rx};
                busy  <= 1'b0;
            end
        end else cnt <= cnt - 16'd1;
    end
endmodule

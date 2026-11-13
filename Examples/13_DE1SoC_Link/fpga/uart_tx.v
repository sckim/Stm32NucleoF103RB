// uart_tx.v
// UART 송신기 (8N1). start가 1이 되면 data를 한 프레임으로 내보낸다.
module uart_tx #(parameter CLKS_PER_BIT = 434) (
    input  wire       clk,
    input  wire       start,         // 1클럭 펄스
    input  wire [7:0] data,
    output reg        tx = 1'b1,     // 유휴 상태는 High
    output wire       busy
);
    reg [9:0]  sh   = 10'h3FF;   // {정지, 데이터 8비트, 시작}
    reg [3:0]  left = 4'd0;      // 남은 비트 수
    reg [15:0] cnt  = 16'd0;
    assign busy = (left != 4'd0);

    always @(posedge clk) begin
        if (!busy) begin
            tx <= 1'b1;
            if (start) begin
                sh   <= {1'b1, data, 1'b0};
                left <= 4'd10;
                cnt  <= 16'd0;
                tx   <= 1'b0;                 // 시작 비트
            end
        end else if (cnt == CLKS_PER_BIT - 1) begin
            cnt  <= 16'd0;
            left <= left - 4'd1;
            sh   <= {1'b1, sh[9:1]};
            tx   <= (left == 4'd1) ? 1'b1 : sh[1];   // 다음 비트
        end else cnt <= cnt + 16'd1;
    end
endmodule

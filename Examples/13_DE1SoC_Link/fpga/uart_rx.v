// uart_rx.v
// UART 수신기 (8N1). 시작 비트 가운데에서 한 번 더 확인하고,
// 그 뒤 1비트 간격으로 각 데이터 비트의 가운데를 표본화한다.
module uart_rx #(parameter CLKS_PER_BIT = 434) (   // 50 MHz / 115200 bps = 434
    input  wire       clk,
    input  wire       rx,            // 동기화된 수신선
    output reg  [7:0] data  = 8'd0,
    output reg        valid = 1'b0   // 1바이트 수신 완료 시 1클럭 동안 1
);
    localparam IDLE = 2'd0, START = 2'd1, DATA = 2'd2, STOP = 2'd3;
    reg [1:0]  state = IDLE;
    reg [15:0] cnt   = 16'd0;    // 비트 시간 카운터
    reg [2:0]  idx   = 3'd0;     // 받은 데이터 비트 수
    reg [7:0]  sh    = 8'd0;     // 시프트 레지스터

    always @(posedge clk) begin
        valid <= 1'b0;
        case (state)
            IDLE: begin
                cnt <= 16'd0;
                idx <= 3'd0;
                if (rx == 1'b0) state <= START;          // 하강 엣지 = 시작 비트 후보
            end
            START: begin                                  // 반 비트 기다린 뒤 다시 확인
                if (cnt == (CLKS_PER_BIT/2) - 1) begin
                    cnt   <= 16'd0;
                    state <= (rx == 1'b0) ? DATA : IDLE;  // 잡음이면 버린다
                end else cnt <= cnt + 16'd1;
            end
            DATA: begin                                   // 1비트마다 가운데에서 표본화
                if (cnt == CLKS_PER_BIT - 1) begin
                    cnt <= 16'd0;
                    sh  <= {rx, sh[7:1]};                 // LSB가 먼저 들어온다
                    idx <= idx + 3'd1;
                    if (idx == 3'd7) state <= STOP;
                end else cnt <= cnt + 16'd1;
            end
            STOP: begin                                   // 정지 비트 가운데
                if (cnt == CLKS_PER_BIT - 1) begin
                    cnt   <= 16'd0;
                    state <= IDLE;
                    if (rx == 1'b1) begin                 // 정지 비트가 1일 때만 유효
                        data  <= sh;
                        valid <= 1'b1;
                    end
                end else cnt <= cnt + 16'd1;
            end
        endcase
    end
endmodule

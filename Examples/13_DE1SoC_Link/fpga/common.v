// common.v
// 2단 플립플롭 동기화기: 비동기 입력을 CLOCK_50 영역으로 옮긴다
module sync2 #(parameter INIT = 1'b1) (
    input  wire clk,
    input  wire d,      // 비동기 입력 (MCU가 만든 신호)
    output wire q       // clk에 동기된 출력
);
    reg [1:0] ff = {2{INIT}};
    always @(posedge clk) ff <= {ff[0], d};
    assign q = ff[1];
endmodule

// 4비트 값 -> 7세그먼트 (DE1-SoC HEX는 0일 때 켜진다)
module hex7seg (
    input  wire [3:0] x,
    output reg  [6:0] seg   // seg[0]=a ... seg[6]=g
);
    always @(*) begin
        case (x)
            4'h0: seg = 7'b1000000;  4'h1: seg = 7'b1111001;
            4'h2: seg = 7'b0100100;  4'h3: seg = 7'b0110000;
            4'h4: seg = 7'b0011001;  4'h5: seg = 7'b0010010;
            4'h6: seg = 7'b0000010;  4'h7: seg = 7'b1111000;
            4'h8: seg = 7'b0000000;  4'h9: seg = 7'b0010000;
            4'hA: seg = 7'b0001000;  4'hB: seg = 7'b0000011;
            4'hC: seg = 7'b1000110;  4'hD: seg = 7'b0100001;
            4'hE: seg = 7'b0000110;  default: seg = 7'b0001110;
        endcase
    end
endmodule

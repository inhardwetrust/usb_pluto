`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 29.09.2025 19:35:57
// Design Name: 
// Module Name: ad936x_ddr_rx_stub
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////

// ad936x_ddr_rx_stub.v
// ????: rx_clk_in ~8 ???. ?????? ????? ?? 4 -> clk_dt ~2 ???.
// ?? clk_dt: negedge -> I (frame=0), posedge -> Q (frame=1).

module ad936x_ddr_rx_stub #
(
  parameter [11:0] I_VAL = 12'h123,
  parameter [11:0] Q_VAL = 12'h3A5
)
(
  input  wire rx_clk_in,    // 8 MHz
  input  wire rx_resetn,
  output wire [11:0] mydata,
  output wire        rx_frame
);

//4 ???????? (50% duty)
  reg [1:0] div=0; reg clk_d=0;
  always @(posedge rx_clk_in or negedge rx_resetn) begin
    if(!rx_resetn) begin div<=0; clk_d<=1'b0; end
    else begin
      div <= div + 1'b1;
      if(div==2'd1) begin div<=0; clk_d<=~clk_d; end
    end
  end

  // ?????????? ???????? ??? I ? Q
  reg [11:0] i_reg, q_reg;
  always @(negedge clk_d or negedge rx_resetn)
    if(!rx_resetn) i_reg <= 12'd0; else i_reg <= I_VAL;

  always @(posedge clk_d or negedge rx_resetn)
    if(!rx_resetn) q_reg <= 12'd0; else q_reg <= Q_VAL;

  // ?????? ???????? ? ???????? ???????????; ???? ??????? ?? mydata
  assign mydata   = clk_d ? q_reg : i_reg; // poslevel -> Q, neglevel -> I
  assign rx_frame = clk_d;                 // 0=I, 1=Q

endmodule
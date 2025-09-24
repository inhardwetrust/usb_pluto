module axis_gen32 #
(
  parameter integer BYTES_PER_BLOCK = 64
)
(
  input  wire        aclk,
  input  wire        aresetn,
  input  wire        s2mm_prmry_resetn,   // 1 = S2MM channel is actually running
  output wire [31:0] tdata,
  output wire        tvalid,
  input  wire        tready,
  output wire        tlast,
  output wire [3:0]  tkeep
);

localparam integer WORDS_PER_BLOCK = BYTES_PER_BLOCK/4;

assign tkeep = 4'hF;  // all 4 bytes valid every beat

reg [31:0] data_r;
reg [7:0]  cnt;
reg        valid_r;
reg        last_pending;   // we are at the last word; hold until hs

wire hs   = valid_r && tready;
wire en   = s2mm_prmry_resetn;
wire [7:0] cnt_next = cnt + 8'd1;

assign tdata  = data_r;
assign tvalid = valid_r;
assign tlast  = valid_r && last_pending;   // keep TLAST asserted until hs occurs

always @(posedge aclk) begin
  if (!aresetn) begin
    valid_r      <= 1'b0;
    last_pending <= 1'b0;
    cnt          <= 8'd0;
    data_r       <= {8'hAA,8'hAA,8'hAA,8'd0};
  end else if (!en) begin
    // S2MM channel not started - keep silent
    valid_r      <= 1'b0;
    last_pending <= 1'b0;
    cnt          <= 8'd0;
    data_r       <= {8'hAA,8'hAA,8'hAA,8'd0};
  end else begin
    // start of frame
    if (!valid_r) begin
      valid_r      <= 1'b1;
      last_pending <= (WORDS_PER_BLOCK==1);
      cnt          <= 8'd0;
      data_r       <= {8'hAA,8'hAA,8'hAA,8'd0}; // first beat is already on the bus
    end else if (hs) begin
      // handshake of the current word
      if (last_pending) begin
        // this was the last beat - frame done
        valid_r      <= 1'b0;
        last_pending <= 1'b0;
        cnt          <= 8'd0;
        data_r       <= {8'hAA,8'hAA,8'hAA,8'd0};
      end else begin
        // prepare the next word
        data_r       <= {8'hAA,8'hAA,8'hAA, cnt_next};
        last_pending <= (cnt_next == WORDS_PER_BLOCK-1);
        cnt          <= cnt_next;
      end
    end
    // when !hs: hold data_r/last_pending/valid_r stable (AXI-Stream stability rule)
  end
end
endmodule

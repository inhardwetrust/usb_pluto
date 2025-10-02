module ad936x_rx_to_axis32 #
(
  parameter integer BYTES_PER_BLOCK = 512  // must be multiple of 4
)
(
  input  wire        rx_clk_in,     // AD936x DATA_CLK
  input  wire        rx_resetn,     // active-low, synchronous to rx_clk_in
  input  wire [11:0] ad_rx_in,      // DDR data (sampled on both edges)
  input  wire        rx_frame,      // 0 = I, 1 = Q (on posedge refers to posedge sample)

  // AXI4-Stream master
  output wire [31:0] m_axis_tdata,
  output wire        m_axis_tvalid,
  input  wire        m_axis_tready,
  output wire [3:0]  m_axis_tkeep,
  output wire        m_axis_tlast
);

  // Always all 4 bytes valid
  assign m_axis_tkeep = 4'hF;

  // --- DDR capture (for real hardware should be replaced with IDDR/ISERDES) ---
  reg [11:0] d_pos, d_neg;
  reg        f_pos, f_neg;

  always @(posedge rx_clk_in) begin
    d_pos <= ad_rx_in;
    f_pos <= rx_frame;
  end

  always @(negedge rx_clk_in) begin
    d_neg <= ad_rx_in;
    f_neg <= rx_frame;
  end

  // Sign-extend 12 -> 16
  function [15:0] sx12to16;
    input [11:0] x;
    begin
      sx12to16 = {{4{x[11]}}, x};
    end
  endfunction

  // On each posedge form "next word" from (pos,neg) pair.
  // If FRAME on posedge = 0 ? pos=I, neg=Q; if =1 ? pos=Q, neg=I.
  reg [31:0] next_word;
  always @(posedge rx_clk_in) begin
    if (!rx_resetn) begin
      next_word <= 32'hAAAA_AA00;
    end else begin
      if (f_pos == 1'b0) begin
        // I on posedge, Q on negedge
        next_word <= { sx12to16(d_pos), sx12to16(d_neg) }; // {I,Q}
      end else begin
        // Q on posedge, I on negedge
        next_word <= { sx12to16(d_neg), sx12to16(d_pos) }; // {I,Q}
      end
    end
  end

  // --- AXIS generation with proper handshake ---
  localparam integer WORDS_PER_BLOCK = BYTES_PER_BLOCK/4;

  reg [31:0] tdata_r;
  reg        tvalid_r;
  reg        last_pending;
  reg [31:0] word_cnt;

  wire hs = tvalid_r && m_axis_tready;

  assign m_axis_tdata  = tdata_r;
  assign m_axis_tvalid = tvalid_r;
  assign m_axis_tlast  = tvalid_r && last_pending;

  always @(posedge rx_clk_in) begin
    if (!rx_resetn) begin
      tvalid_r     <= 1'b0;
      last_pending <= 1'b0;
      word_cnt     <= 32'd0;
      tdata_r      <= 32'hAAAA_AA00;
    end else begin
      if (!tvalid_r) begin
        // Start of streaming - place first word and raise TVALID
        tvalid_r     <= 1'b1;
        tdata_r      <= next_word;
        word_cnt     <= 32'd0;
        last_pending <= (WORDS_PER_BLOCK == 1);
      end else if (hs) begin
        // Current word accepted - prepare the next one
        if (last_pending) begin
          // End of block
          tdata_r      <= next_word;         // immediately start next block (continuous stream)
          word_cnt     <= 32'd0;
          last_pending <= (WORDS_PER_BLOCK == 1);
          // tvalid_r stays 1 - no gaps between blocks
        end else begin
          tdata_r      <= next_word;
          word_cnt     <= word_cnt + 32'd1;
          last_pending <= ((word_cnt + 32'd1) == (WORDS_PER_BLOCK-1));
        end
      end
      // when !hs - hold tdata_r/tvalid_r/last_pending stable (AXIS stability rule)
    end
  end

  // (optional) compile-time guard
  // synopsys translate_off
  initial begin
    if (BYTES_PER_BLOCK % 4 != 0)
      $error("BYTES_PER_BLOCK must be multiple of 4");
  end
  // synopsys translate_on

endmodule

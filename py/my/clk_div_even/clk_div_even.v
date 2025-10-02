// clk_div_even.v — even divider with 50% duty cycle
module clk_div_even #(
    parameter integer DIV = 4   // must be even: 2,4,6,...
) (
    input  wire clk_in,           // fast clock input (e.g., 100 MHz)
    input  wire rstn,             // active-low reset
    output wire clk_out           // slower system clock output
);

    // safety check: only even division factors are supported
    initial if (DIV % 2 != 0) $error("clk_div_even: DIV must be even");

    localparam integer HALF = DIV/2;
    localparam integer W = $clog2(HALF);

    reg [W-1:0] cnt = {W{1'b0}};
    reg q = 1'b0;

    // Toggle the output every HALF cycles of the input clock
    always @(posedge clk_in or negedge rstn) begin
        if (!rstn) begin
            cnt <= 0;
            q   <= 1'b0;
        end else if (cnt == HALF-1) begin
            cnt <= 0;
            q   <= ~q;  // output toggles every HALF cycles -> full period = DIV
        end else begin
            cnt <= cnt + 1'b1;
        end
    end

    wire clk_div_raw = q;

    // Drive the divided clock onto the global clock network
    BUFG u_bufg (
        .I(clk_div_raw),
        .O(clk_out)
    );
endmodule

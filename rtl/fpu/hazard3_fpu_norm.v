// Combined leading-zero count and left-normalise, binary-search style
// (log2(W) stages of zero-detect + mux). Output is undefined for in == 0.
// SPDX-License-Identifier: Apache-2.0

`default_nettype none

module hazard3_fpu_norm #(
	parameter W  = 52,
	parameter WC = 6    // 2**WC - 1 >= W - 1
) (
	input  wire [W-1:0]  in,
	output reg  [W-1:0]  out,
	output reg  [WC-1:0] lz
);

integer j;
reg [W-1:0] x;

always @ (*) begin
	x  = in;
	lz = {WC{1'b0}};
	for (j = WC - 1; j >= 0; j = j - 1) begin
		if ((x >> (W - (1 << j))) == {W{1'b0}}) begin
			x  = x << (1 << j);
			lz = lz | (1 << j);
		end
	end
	out = x;
end

endmodule

`default_nettype wire

// Classify an IEEE binary32 value and normalise its significand, so that
// value = mant * 2**(exp - 23) with mant[23] == 1 for all finite non-zero
// inputs (subnormals included). exp is a 12-bit two's complement number.
// SPDX-License-Identifier: Apache-2.0

`default_nettype none

module hazard3_fpu_unpack (
	input  wire [31:0] x,
	output wire        sign,
	output wire        zero,
	output wire        subnormal,
	output wire        inf,
	output wire        nan,
	output wire        snan,
	output wire [23:0] mant,
	output wire [11:0] exp
);

wire [7:0]  e = x[30:23];
wire [22:0] f = x[22:0];

assign sign      = x[31];
assign zero      = e == 8'h00 && f == 23'h0;
assign subnormal = e == 8'h00 && f != 23'h0;
assign inf       = e == 8'hff && f == 23'h0;
assign nan       = e == 8'hff && f != 23'h0;
assign snan      = nan && !f[22];

wire [4:0] lz;

hazard3_fpu_norm #(
	.W  (24),
	.WC (5)
) norm_u (
	.in  ({e != 8'h00, f}),
	.out (mant),
	.lz  (lz)
);

// Subnormals use exponent field 1, then subtract the normalisation shift.
assign exp = {4'h0, (e == 8'h00 ? 8'h01 : e)} - 12'd127 - {7'h0, lz};

endmodule

`default_nettype wire

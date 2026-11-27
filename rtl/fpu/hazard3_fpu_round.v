// Round and pack: (-1)**sign * 1.mant[22:0] * 2**exp, plus round bit and
// sticky bit below mant[0], to IEEE binary32 under any RISC-V rounding mode.
// Handles overflow and gradual underflow. Tininess is detected after
// rounding, as required by RISC-V. Input must not be zero.
// SPDX-License-Identifier: Apache-2.0

`default_nettype none

module hazard3_fpu_round (
	input  wire        sign,
	input  wire [11:0] exp,     // two's complement, unbiased
	input  wire [23:0] mant,    // mant[23] == 1
	input  wire        rbit,
	input  wire        sticky,
	input  wire [2:0]  rm,
	output reg  [31:0] result,
	output reg  [4:0]  flags    // {NV, DZ, OF, UF, NX}
);

`include "hazard3_fpu_ops.vh"

function round_inc;
	input [2:0] mode;
	input       s;
	input       lsb;
	input       r;
	input       st;
begin
	case (mode)
		RM_RNE:  round_inc = r && (st || lsb);
		RM_RTZ:  round_inc = 1'b0;
		RM_RDN:  round_inc = (r || st) && s;
		RM_RUP:  round_inc = (r || st) && !s;
		RM_RMM:  round_inc = r;
		default: round_inc = 1'b0;
	endcase
end
endfunction

wire [11:0] bexp = exp + 12'd127;
wire        bexp_le0 = $signed(bexp) <= 0;

// Normal-range rounding (also used to decide tininess after rounding)
wire        inc_n   = round_inc(rm, sign, mant[0], rbit, sticky);
wire [24:0] mant_n  = {1'b0, mant} + {24'h0, inc_n};
wire [11:0] bexp_n  = bexp + {11'h0, mant_n[24]};
wire        ovf     = $signed(bexp_n) >= 255;

// Subnormal rounding: denormalise by (1 - bexp), at most 26 places
wire [11:0] sh_full = 12'd1 - bexp;
wire [4:0]  sh      = $signed(sh_full) > 26 ? 5'd26 : sh_full[4:0];
wire [50:0] den     = {mant, rbit, 26'h0} >> sh;
wire [23:0] mant_s  = den[50:27];
wire        r_s     = den[26];
wire        st_s    = sticky || |den[25:0];
wire        inc_s   = round_inc(rm, sign, mant_s[0], r_s, st_s);
wire [23:0] mant_sr = mant_s + {23'h0, inc_s};   // may carry into bit 23 -> min normal
wire        tiny    = $signed(bexp) < 0 || (bexp == 12'h0 && !mant_n[24]);

wire ovf_to_inf = rm == RM_RNE || rm == RM_RMM ||
                  (rm == RM_RUP && !sign) || (rm == RM_RDN && sign);

always @ (*) begin
	flags = 5'h0;
	if (bexp_le0) begin
		result = {sign, 7'h0, mant_sr};
		flags[FLAG_NX] = r_s || st_s;
		flags[FLAG_UF] = tiny && (r_s || st_s);
	end else if (ovf) begin
		result = ovf_to_inf ? {sign, 8'hff, 23'h0} : {sign, 8'hfe, 23'h7fffff};
		flags[FLAG_OF] = 1'b1;
		flags[FLAG_NX] = 1'b1;
	end else begin
		result = {sign, bexp_n[7:0], mant_n[22:0]};
		flags[FLAG_NX] = rbit || sticky;
	end
end

endmodule

`default_nettype wire

// RV32F / Zfinx single-precision FPU for Hazard3.
//
// One operation at a time, multi-cycle, with the same valid/ready/kill
// handshake style as hazard3_muldiv_seq so it can sit beside it in stage X.
//
//   op_vld && op_rdy : operands are latched, operation starts
//   op_kill          : abandon the current operation (e.g. trap in M);
//                      op_vld in the same cycle starts a new one at once
//   result_vld       : result/fflags valid; stays high until the next op
//
// Latency (cycles after the accept cycle):
//   sign-inject, min/max, compare, classify ......... 1
//   FCVT.W[U].S ...................................... 2
//   FCVT.S.W[U] ...................................... 3
//   FADD/FSUB/FMUL/FMADD/FMSUB/FNMSUB/FNMADD ......... 6
//   FDIV, FSQRT ...................................... 27
//   (special operands such as NaN/inf/zero finish in 1)
//
// Internally all finite non-zero operands are normalised (subnormals too),
// and every arithmetic result goes through one shared round-and-pack stage.
//
// SPDX-License-Identifier: Apache-2.0

`default_nettype none

module hazard3_fpu (
	input  wire        clk,
	input  wire        rst_n,

	input  wire        op_vld,
	output wire        op_rdy,
	input  wire        op_kill,
	input  wire [4:0]  op,
	input  wire [2:0]  rm,       // resolved rounding mode (never DYN)
	input  wire [31:0] op_a,
	input  wire [31:0] op_b,
	input  wire [31:0] op_c,

	output reg  [31:0] result,
	output reg  [4:0]  fflags,   // {NV, DZ, OF, UF, NX}
	output wire        result_vld
);

`include "hazard3_fpu_ops.vh"

localparam [3:0] S_IDLE   = 4'd0;
localparam [3:0] S_UNPACK = 4'd1;
localparam [3:0] S_MUL    = 4'd2;
localparam [3:0] S_ALIGN  = 4'd3;
localparam [3:0] S_ADD    = 4'd4;
localparam [3:0] S_NORM   = 4'd5;
localparam [3:0] S_ROUND  = 4'd6;
localparam [3:0] S_DIV    = 4'd7;
localparam [3:0] S_SQRT   = 4'd8;
localparam [3:0] S_CVTI   = 4'd9;
localparam [3:0] S_DONE   = 4'd10;

reg [3:0] state;

assign op_rdy     = state == S_IDLE || state == S_DONE;
assign result_vld = state == S_DONE;

// As hazard3_muldiv_seq: a kill may coincide with a new op, which then
// starts immediately regardless of op_rdy.
wire accept = op_vld && (op_rdy || op_kill);

// ----------------------------------------------------------------------------
// Operand mapping at issue. Everything in the add/mul family becomes a
// fused multiply-add a * b + c:
//   FADD a, b  ->  a * 1.0 + b        FMUL a, b  ->  a * b + (+/-0)
//   sign flips for FSUB/FMSUB/FNMSUB/FNMADD are applied to a and c.
// For MUL the zero addend carries the product sign, so an exact-zero product
// keeps its sign under every rounding mode. Other ops see a, b unchanged.

wire op_addsub = op == FPU_ADD || op == FPU_SUB;
wire neg_prod  = op == FPU_NMSUB || op == FPU_NMADD;
wire neg_c     = op == FPU_MSUB  || op == FPU_NMADD;

wire [31:0] map_a = op_a ^ {neg_prod, 31'h0};
wire [31:0] map_b = op_addsub ? 32'h3f800000 : op_b;
wire [31:0] map_c =
	op_addsub     ? op_b ^ {op == FPU_SUB, 31'h0} :
	op == FPU_MUL ? {op_a[31] ^ op_b[31], 31'h0}  :
	                op_c ^ {neg_c, 31'h0};

reg [4:0]  r_op;
reg [2:0]  r_rm;
reg [31:0] r_a;
reg [31:0] r_b;
reg [31:0] r_c;

// ----------------------------------------------------------------------------
// Unpack (combinational from latched operands, used in S_UNPACK)

wire        ua_sign, ua_zero, ua_sub, ua_inf, ua_nan, ua_snan;
wire        ub_sign, ub_zero, ub_sub, ub_inf, ub_nan, ub_snan;
wire        uc_sign, uc_zero, uc_sub, uc_inf, uc_nan, uc_snan;
wire [23:0] ua_mant, ub_mant, uc_mant;
wire [11:0] ua_exp,  ub_exp,  uc_exp;

hazard3_fpu_unpack unpack_a (
	.x (r_a), .sign (ua_sign), .zero (ua_zero), .subnormal (ua_sub), .inf (ua_inf),
	.nan (ua_nan), .snan (ua_snan), .mant (ua_mant), .exp (ua_exp)
);

hazard3_fpu_unpack unpack_b (
	.x (r_b), .sign (ub_sign), .zero (ub_zero), .subnormal (ub_sub), .inf (ub_inf),
	.nan (ub_nan), .snan (ub_snan), .mant (ub_mant), .exp (ub_exp)
);

hazard3_fpu_unpack unpack_c (
	.x (r_c), .sign (uc_sign), .zero (uc_zero), .subnormal (uc_sub), .inf (uc_inf),
	.nan (uc_nan), .snan (uc_snan), .mant (uc_mant), .exp (uc_exp)
);

// ----------------------------------------------------------------------------
// Compare / min / max / classify (all combinational, finish in S_UNPACK)

wire any_nan_ab  = ua_nan  || ub_nan;
wire any_snan_ab = ua_snan || ub_snan;
wire both_zero   = ua_zero && ub_zero;
wire cmp_eq      = r_a == r_b || both_zero;

// a < b and b < a for non-NaN operands (sign-magnitude compare)
wire mag_a_lt_b  = r_a[30:0] < r_b[30:0];
wire mag_b_lt_a  = r_b[30:0] < r_a[30:0];
wire cmp_lt =
	both_zero               ? 1'b0       :
	ua_sign != ub_sign      ? ua_sign    :
	ua_sign                 ? mag_b_lt_a : mag_a_lt_b;
wire cmp_gt =
	both_zero               ? 1'b0       :
	ua_sign != ub_sign      ? ub_sign    :
	ua_sign                 ? mag_a_lt_b : mag_b_lt_a;

wire [9:0] fclass = {
	ua_nan && !ua_snan,                       // 9 quiet NaN
	ua_snan,                                  // 8 signalling NaN
	!ua_sign && ua_inf,                       // 7 +inf
	!ua_sign && !ua_zero && !ua_sub && !ua_inf && !ua_nan, // 6 +normal
	!ua_sign && ua_sub,                       // 5 +subnormal
	!ua_sign && ua_zero,                      // 4 +0
	ua_sign  && ua_zero,                      // 3 -0
	ua_sign  && ua_sub,                       // 2 -subnormal
	ua_sign  && !ua_zero && !ua_sub && !ua_inf && !ua_nan, // 1 -normal
	ua_sign  && ua_inf                        // 0 -inf
};

// ----------------------------------------------------------------------------
// S_UNPACK decisions: special-case results, or which datapath to run next

wire prod_sign  = ua_sign ^ ub_sign;
wire prod_zero  = ua_zero || ub_zero;
wire prod_0xinf = (ua_zero && ub_inf) || (ua_inf && ub_zero);
wire prod_inf   = (ua_inf || ub_inf) && !prod_0xinf;
wire fma_nan    = ua_nan || ub_nan || uc_nan || prod_0xinf;
wire fma_nv     = ua_snan || ub_snan || uc_snan || prod_0xinf;

// Sign of an exact zero sum: same-signed zeros keep their sign, otherwise
// +0, except -0 when rounding down.
wire zero_sum_sign = prod_sign == uc_sign ? uc_sign : r_rm == RM_RDN;

// Integer source magnitude for FCVT.S.W[U]
wire        cvtf_neg = r_op == FPU_CVT_S_W && r_a[31];
wire [31:0] cvtf_mag = cvtf_neg ? -r_a : r_a;

reg        unp_finish;
reg [31:0] unp_result;
reg [4:0]  unp_flags;
reg [3:0]  unp_next;

always @ (*) begin
	unp_finish = 1'b1;
	unp_result = 32'h0;
	unp_flags  = 5'h0;
	unp_next   = S_DONE;
	case (r_op)
	FPU_SGNJ:  unp_result = {r_b[31],            r_a[30:0]};
	FPU_SGNJN: unp_result = {!r_b[31],           r_a[30:0]};
	FPU_SGNJX: unp_result = {r_a[31] ^ r_b[31],  r_a[30:0]};

	FPU_MIN, FPU_MAX: begin
		unp_flags[FLAG_NV] = any_snan_ab;
		if (ua_nan && ub_nan)
			unp_result = F32_CANONICAL_NAN;
		else if (ua_nan)
			unp_result = r_b;
		else if (ub_nan)
			unp_result = r_a;
		else if (r_op == FPU_MIN)
			unp_result = cmp_lt || (cmp_eq &&  ua_sign) ? r_a : r_b;
		else
			unp_result = cmp_gt || (cmp_eq && !ua_sign) ? r_a : r_b;
	end

	FPU_EQ: begin
		unp_flags[FLAG_NV] = any_snan_ab;
		unp_result = {31'h0, !any_nan_ab && cmp_eq};
	end
	FPU_LT: begin
		unp_flags[FLAG_NV] = any_nan_ab;
		unp_result = {31'h0, !any_nan_ab && cmp_lt};
	end
	FPU_LE: begin
		unp_flags[FLAG_NV] = any_nan_ab;
		unp_result = {31'h0, !any_nan_ab && (cmp_lt || cmp_eq)};
	end

	FPU_CLASS: unp_result = {22'h0, fclass};

	FPU_ADD, FPU_SUB, FPU_MUL, FPU_MADD, FPU_MSUB, FPU_NMSUB, FPU_NMADD: begin
		if (fma_nan) begin
			unp_result = F32_CANONICAL_NAN;
			unp_flags[FLAG_NV] = fma_nv;
		end else if (prod_inf) begin
			if (uc_inf && uc_sign != prod_sign) begin
				unp_result = F32_CANONICAL_NAN;
				unp_flags[FLAG_NV] = 1'b1;
			end else begin
				unp_result = {prod_sign, 8'hff, 23'h0};
			end
		end else if (uc_inf) begin
			unp_result = r_c;
		end else if (prod_zero) begin
			unp_result = uc_zero ? {zero_sum_sign, 31'h0} : r_c;
		end else begin
			unp_finish = 1'b0;
			unp_next   = S_MUL;
		end
	end

	FPU_DIV: begin
		if (any_nan_ab || (ua_inf && ub_inf) || both_zero) begin
			unp_result = F32_CANONICAL_NAN;
			unp_flags[FLAG_NV] = any_snan_ab || !any_nan_ab;
		end else if (ua_inf) begin
			unp_result = {prod_sign, 8'hff, 23'h0};
		end else if (ub_zero) begin
			unp_result = {prod_sign, 8'hff, 23'h0};
			unp_flags[FLAG_DZ] = 1'b1;
		end else if (ua_zero || ub_inf) begin
			unp_result = {prod_sign, 31'h0};
		end else begin
			unp_finish = 1'b0;
			unp_next   = S_DIV;
		end
	end

	FPU_SQRT: begin
		if (ua_nan) begin
			unp_result = F32_CANONICAL_NAN;
			unp_flags[FLAG_NV] = ua_snan;
		end else if (ua_zero) begin
			unp_result = r_a;
		end else if (ua_sign) begin
			unp_result = F32_CANONICAL_NAN;
			unp_flags[FLAG_NV] = 1'b1;
		end else if (ua_inf) begin
			unp_result = r_a;
		end else begin
			unp_finish = 1'b0;
			unp_next   = S_SQRT;
		end
	end

	FPU_CVT_W_S, FPU_CVT_WU_S: begin
		if (ua_nan || ua_inf) begin
			unp_flags[FLAG_NV] = 1'b1;
			if (r_op == FPU_CVT_W_S)
				unp_result = ua_inf && ua_sign ? 32'h80000000 : 32'h7fffffff;
			else
				unp_result = ua_inf && ua_sign ? 32'h00000000 : 32'hffffffff;
		end else begin
			unp_finish = 1'b0;
			unp_next   = S_CVTI;
		end
	end

	FPU_CVT_S_W, FPU_CVT_S_WU: begin
		if (r_a == 32'h0) begin
			unp_result = 32'h0;
		end else begin
			unp_finish = 1'b0;
			unp_next   = S_NORM;
		end
	end

	default: begin
		unp_result = F32_CANONICAL_NAN;
	end
	endcase
end

// ----------------------------------------------------------------------------
// Unpacked operand registers

reg        u_sa, u_sb, u_sc, u_za, u_zc;
reg [23:0] u_ma, u_mb, u_mc;
reg [11:0] u_ea, u_eb, u_ec;

// ----------------------------------------------------------------------------
// FMA datapath
//
// Fixed-point format for both addends: M * 2**(E - 49), M 52 bits wide.
//   product: M = (ma * mb) << 3,  E = ea + eb
//   addend:  M =  mc << 26,       E = ec
// The 3 extra LSBs keep guard bits below the 48-bit product, and anything
// shifted out during alignment is jammed into the LSB (sticky).

reg [47:0] f_prod;
reg [11:0] f_ep;

wire [51:0] al_pm = {1'b0, f_prod, 3'h0};
wire [51:0] al_cm = {2'h0, u_mc, 26'h0};
wire [12:0] al_ediff = {f_ep[11], f_ep} - {u_ec[11], u_ec};
wire        al_prod_big = u_zc || !al_ediff[12];
wire [12:0] al_dist = al_prod_big ? al_ediff : -al_ediff;
wire [5:0]  al_sh = u_zc || al_dist > 13'd63 ? 6'd63 : al_dist[5:0];
wire [51:0] al_small_raw = u_zc ? 52'h0 : al_prod_big ? al_cm : al_pm;
wire [51:0] al_small_sh  = al_small_raw >> al_sh;
wire        al_jam = |(al_small_raw & ~({52{1'b1}} << al_sh));

reg [51:0] f_big;
reg [51:0] f_small;
reg [11:0] f_ebig;
reg        f_sbig;
reg        f_ssmall;
reg        f_effsub;

wire [51:0] ad_sum  = f_big + f_small;
wire [52:0] ad_d1   = {1'b0, f_big} - {1'b0, f_small};
wire [51:0] ad_d2   = f_small - f_big;
wire        ad_neg  = ad_d1[52];
wire [51:0] ad_res  = !f_effsub ? ad_sum : ad_neg ? ad_d2 : ad_d1[51:0];
wire        ad_sign = f_effsub && ad_neg ? f_ssmall : f_sbig;

// Normalise stage, shared by FMA and FCVT.S.W[U]
reg [51:0] n_m;
reg [11:0] n_e;
reg        n_sign;
reg        n_zero;

wire [51:0] nm_out;
wire [5:0]  nm_lz;

hazard3_fpu_norm #(
	.W  (52),
	.WC (6)
) norm_u (
	.in  (n_m),
	.out (nm_out),
	.lz  (nm_lz)
);

// ----------------------------------------------------------------------------
// Divide / square root: radix-2 restoring, one result bit per cycle,
// 25 bits = 24-bit significand + round bit, remainder gives sticky.

reg [27:0] ds_rem;
reg [24:0] ds_q;
reg [49:0] ds_x;
reg [4:0]  ds_cnt;

wire        dv_ge       = ds_rem[25:0] >= {2'h0, u_mb};
wire [25:0] dv_rem_sub  = dv_ge ? ds_rem[25:0] - {2'h0, u_mb} : ds_rem[25:0];
wire [24:0] dv_q_next   = {ds_q[23:0], dv_ge};

wire [27:0] sq_rem_t    = {ds_rem[25:0], ds_x[49:48]};
wire [27:0] sq_trial    = {1'b0, ds_q, 2'b01};
wire        sq_ge       = sq_rem_t >= sq_trial;
wire [27:0] sq_rem_next = sq_ge ? sq_rem_t - sq_trial : sq_rem_t;
wire [24:0] sq_q_next   = {ds_q[23:0], sq_ge};

wire [11:0] sq_exp_even = ua_exp - {11'h0, ua_exp[0]};

// ----------------------------------------------------------------------------
// Round-and-pack stage inputs

reg        rd_sign;
reg [11:0] rd_exp;
reg [23:0] rd_mant;
reg        rd_r;
reg        rd_s;
reg        rd_zero;

wire [31:0] rnd_result;
wire [4:0]  rnd_flags;

hazard3_fpu_round round_u (
	.sign   (rd_sign),
	.exp    (rd_exp),
	.mant   (rd_mant),
	.rbit   (rd_r),
	.sticky (rd_s),
	.rm     (r_rm),
	.result (rnd_result),
	.flags  (rnd_flags)
);

// ----------------------------------------------------------------------------
// FCVT.W.S / FCVT.WU.S (from unpacked registers)

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

wire        ci_big  = !u_za && $signed(u_ea) > 31;
wire        ci_tiny = !u_za && $signed(u_ea) < -1;
wire [11:0] ci_sh_w = 12'd31 - u_ea;
wire [63:0] ci_f    = {u_ma, 40'h0} >> ci_sh_w[5:0];
wire        ci_valid_sh = !u_za && !ci_big && !ci_tiny;
wire [31:0] ci_int  = ci_valid_sh ? ci_f[63:32] : 32'h0;
wire        ci_r    = ci_valid_sh ? ci_f[31]    : 1'b0;
wire        ci_s    = ci_valid_sh ? |ci_f[30:0] : ci_tiny;
wire        ci_inc  = round_inc(r_rm, u_sa, ci_int[0], ci_r, ci_s);
wire [32:0] ci_rnd  = {1'b0, ci_int} + {32'h0, ci_inc};

reg [31:0] ci_result;
reg [4:0]  ci_flags;

always @ (*) begin
	ci_flags = 5'h0;
	if (r_op == FPU_CVT_W_S) begin
		if (ci_big || (!u_sa && ci_rnd > 33'h07fffffff) || (u_sa && ci_rnd > 33'h080000000)) begin
			ci_result = u_sa ? 32'h80000000 : 32'h7fffffff;
			ci_flags[FLAG_NV] = 1'b1;
		end else begin
			ci_result = u_sa ? -ci_rnd[31:0] : ci_rnd[31:0];
			ci_flags[FLAG_NX] = ci_r || ci_s;
		end
	end else begin
		if (u_sa && (ci_big || ci_rnd != 33'h0)) begin
			ci_result = 32'h0;
			ci_flags[FLAG_NV] = 1'b1;
		end else if (ci_big || ci_rnd[32]) begin
			ci_result = 32'hffffffff;
			ci_flags[FLAG_NV] = 1'b1;
		end else begin
			ci_result = ci_rnd[31:0];
			ci_flags[FLAG_NX] = ci_r || ci_s;
		end
	end
end

// ----------------------------------------------------------------------------
// State machine

always @ (posedge clk or negedge rst_n) begin
	if (!rst_n) begin
		state    <= S_IDLE;
		result   <= 32'h0;
		fflags   <= 5'h0;
		r_op     <= 5'h0;
		r_rm     <= 3'h0;
		r_a      <= 32'h0;
		r_b      <= 32'h0;
		r_c      <= 32'h0;
		{u_sa, u_sb, u_sc, u_za, u_zc} <= 5'h0;
		{u_ma, u_mb, u_mc} <= {72{1'b0}};
		{u_ea, u_eb, u_ec} <= {36{1'b0}};
		f_prod   <= 48'h0;
		f_ep     <= 12'h0;
		f_big    <= 52'h0;
		f_small  <= 52'h0;
		f_ebig   <= 12'h0;
		f_sbig   <= 1'b0;
		f_ssmall <= 1'b0;
		f_effsub <= 1'b0;
		n_m      <= 52'h0;
		n_e      <= 12'h0;
		n_sign   <= 1'b0;
		n_zero   <= 1'b0;
		ds_rem   <= 28'h0;
		ds_q     <= 25'h0;
		ds_x     <= 50'h0;
		ds_cnt   <= 5'h0;
		rd_sign  <= 1'b0;
		rd_exp   <= 12'h0;
		rd_mant  <= 24'h0;
		rd_r     <= 1'b0;
		rd_s     <= 1'b0;
		rd_zero  <= 1'b0;
	end else if (accept) begin
		r_op  <= op;
		r_rm  <= rm;
		r_a   <= map_a;
		r_b   <= map_b;
		r_c   <= map_c;
		state <= S_UNPACK;
	end else if (op_kill) begin
		state <= S_IDLE;
	end else begin
		case (state)
		S_IDLE, S_DONE: begin
			// Waiting for an operation (accepted above)
		end

		S_UNPACK: begin
			u_sa <= ua_sign; u_sb <= ub_sign; u_sc <= uc_sign;
			u_za <= ua_zero; u_zc <= uc_zero;
			u_ma <= ua_mant; u_mb <= ub_mant; u_mc <= uc_mant;
			u_ea <= ua_exp;  u_eb <= ub_exp;  u_ec <= uc_exp;
			if (unp_finish) begin
				result <= unp_result;
				fflags <= unp_flags;
				state  <= S_DONE;
			end else begin
				state <= unp_next;
				if (unp_next == S_DIV) begin
					// Pre-scale so the quotient lies in [1, 2)
					if (ua_mant >= ub_mant) begin
						ds_rem <= {4'h0, ua_mant};
						rd_exp <= ua_exp - ub_exp;
					end else begin
						ds_rem <= {3'h0, ua_mant, 1'b0};
						rd_exp <= ua_exp - ub_exp - 12'd1;
					end
					rd_sign <= prod_sign;
					ds_q    <= 25'h0;
					ds_cnt  <= 5'd24;
				end
				if (unp_next == S_SQRT) begin
					// Make the exponent even; radicand in [1, 4)
					ds_x    <= ua_exp[0] ? {ua_mant, 1'b0, 25'h0} : {1'b0, ua_mant, 25'h0};
					rd_exp  <= {sq_exp_even[11], sq_exp_even[11:1]};
					rd_sign <= 1'b0;
					ds_rem  <= 28'h0;
					ds_q    <= 25'h0;
					ds_cnt  <= 5'd24;
				end
				if (unp_next == S_NORM) begin
					// Integer -> float: value = mag * 2**0 = (mag << 20) * 2**(29 - 49)
					n_m    <= {cvtf_mag, 20'h0};
					n_e    <= 12'd29;
					n_sign <= cvtf_neg;
					n_zero <= 1'b0;
				end
			end
		end

		S_MUL: begin
			f_prod <= u_ma * u_mb;
			f_ep   <= u_ea + u_eb;
			state  <= S_ALIGN;
		end

		S_ALIGN: begin
			f_big    <= al_prod_big ? al_pm : al_cm;
			f_small  <= al_small_sh | {51'h0, al_jam};
			f_ebig   <= al_prod_big ? f_ep : u_ec;
			f_sbig   <= al_prod_big ? u_sa ^ u_sb : u_sc;
			f_ssmall <= al_prod_big ? u_sc : u_sa ^ u_sb;
			f_effsub <= (u_sa ^ u_sb) != u_sc && !u_zc;
			state    <= S_ADD;
		end

		S_ADD: begin
			n_m    <= ad_res;
			n_e    <= f_ebig;
			n_zero <= ad_res == 52'h0;
			n_sign <= ad_res == 52'h0 ? r_rm == RM_RDN : ad_sign;
			state  <= S_NORM;
		end

		S_NORM: begin
			rd_zero <= n_zero;
			rd_sign <= n_sign;
			rd_exp  <= n_e + 12'd2 - {6'h0, nm_lz};
			rd_mant <= nm_out[51:28];
			rd_r    <= nm_out[27];
			rd_s    <= |nm_out[26:0];
			state   <= S_ROUND;
		end

		S_DIV: begin
			ds_rem <= {2'h0, dv_rem_sub[24:0], 1'b0};
			ds_q   <= dv_q_next;
			ds_cnt <= ds_cnt - 5'd1;
			if (ds_cnt == 5'd0) begin
				rd_mant <= dv_q_next[24:1];
				rd_r    <= dv_q_next[0];
				rd_s    <= dv_rem_sub != 26'h0;
				rd_zero <= 1'b0;
				state   <= S_ROUND;
			end
		end

		S_SQRT: begin
			ds_rem <= sq_rem_next;
			ds_q   <= sq_q_next;
			ds_x   <= {ds_x[47:0], 2'h0};
			ds_cnt <= ds_cnt - 5'd1;
			if (ds_cnt == 5'd0) begin
				rd_mant <= sq_q_next[24:1];
				rd_r    <= sq_q_next[0];
				rd_s    <= sq_rem_next != 28'h0;
				rd_zero <= 1'b0;
				state   <= S_ROUND;
			end
		end

		S_ROUND: begin
			result <= rd_zero ? {rd_sign, 31'h0} : rnd_result;
			fflags <= rd_zero ? 5'h0 : rnd_flags;
			state  <= S_DONE;
		end

		S_CVTI: begin
			result <= ci_result;
			fflags <= ci_flags;
			state  <= S_DONE;
		end

		default: state <= S_IDLE;
		endcase
	end
end

endmodule

`default_nettype wire

// Testbench for hazard3_fpu: every result and every fflags bit is compared
// against Berkeley SoftFloat built with its RISC-V specialisation.
//
//   Vhazard3_fpu random <op|all> <count> [seed]
//   Vhazard3_fpu stdin  <op> <rm>          (inputs: TestFloat testfloat_gen lines)
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "Vhazard3_fpu.h"
#include "verilated.h"

extern "C" {
#include "softfloat.h"
}

// Must match rtl/fpu/hazard3_fpu_ops.vh
enum Op {
	ADD, SUB, MUL, MADD, MSUB, NMSUB, NMADD, DIV, SQRT, SGNJ, SGNJN, SGNJX,
	MIN, MAX, EQ, LT, LE, CLASS, CVT_W_S, CVT_WU_S, CVT_S_W, CVT_S_WU, NUM_OPS
};

static const char *op_names[NUM_OPS] = {
	"add", "sub", "mul", "madd", "msub", "nmsub", "nmadd", "div", "sqrt",
	"sgnj", "sgnjn", "sgnjx", "min", "max", "eq", "lt", "le", "class",
	"cvt_w_s", "cvt_wu_s", "cvt_s_w", "cvt_s_wu"
};

static int num_srcs(int op) {
	switch (op) {
	case MADD: case MSUB: case NMSUB: case NMADD:
		return 3;
	case SQRT: case CLASS: case CVT_W_S: case CVT_WU_S: case CVT_S_W: case CVT_S_WU:
		return 1;
	default:
		return 2;
	}
}

static bool uses_rm(int op) {
	return op <= SQRT || op == CVT_W_S || op == CVT_WU_S || op == CVT_S_W || op == CVT_S_WU;
}

static int parse_op(const char *s) {
	for (int i = 0; i < NUM_OPS; ++i)
		if (!strcmp(s, op_names[i]))
			return i;
	return -1;
}

// ----------------------------------------------------------------------------
// Reference model

static inline bool is_nan(uint32_t x)  { return (x & 0x7f800000u) == 0x7f800000u && (x & 0x7fffffu); }
static inline bool is_snan(uint32_t x) { return is_nan(x) && !(x & 0x400000u); }
static inline float32_t F(uint32_t x)  { float32_t f; f.v = x; return f; }

struct Res { uint32_t v; uint32_t flags; };

static uint32_t fclass(uint32_t x) {
	bool s = x >> 31;
	uint32_t e = (x >> 23) & 0xff, f = x & 0x7fffff;
	if (e == 0xff)
		return f ? ((f & 0x400000) ? 1u << 9 : 1u << 8) : (s ? 1u << 0 : 1u << 7);
	if (e == 0)
		return f ? (s ? 1u << 2 : 1u << 5) : (s ? 1u << 3 : 1u << 4);
	return s ? 1u << 1 : 1u << 6;
}

static Res model(int op, int rm, uint32_t a, uint32_t b, uint32_t c) {
	softfloat_roundingMode = rm;
	softfloat_detectTininess = softfloat_tininess_afterRounding;
	softfloat_exceptionFlags = 0;
	const uint32_t S = 0x80000000u;
	uint32_t r = 0;
	switch (op) {
	case ADD:   r = f32_add(F(a), F(b)).v; break;
	case SUB:   r = f32_sub(F(a), F(b)).v; break;
	case MUL:   r = f32_mul(F(a), F(b)).v; break;
	case MADD:  r = f32_mulAdd(F(a), F(b), F(c)).v; break;
	case MSUB:  r = f32_mulAdd(F(a), F(b), F(c ^ S)).v; break;
	case NMSUB: r = f32_mulAdd(F(a ^ S), F(b), F(c)).v; break;
	case NMADD: r = f32_mulAdd(F(a ^ S), F(b), F(c ^ S)).v; break;
	case DIV:   r = f32_div(F(a), F(b)).v; break;
	case SQRT:  r = f32_sqrt(F(a)).v; break;
	case SGNJ:  r = (a & ~S) | (b & S); break;
	case SGNJN: r = (a & ~S) | (~b & S); break;
	case SGNJX: r = a ^ (b & S); break;
	case MIN:
	case MAX: {
		uint32_t flags = (is_snan(a) || is_snan(b)) ? softfloat_flag_invalid : 0;
		if (is_nan(a) && is_nan(b)) {
			r = 0x7fc00000u;
		} else if (is_nan(a)) {
			r = b;
		} else if (is_nan(b)) {
			r = a;
		} else {
			bool eq = f32_eq(F(a), F(b));
			bool pick_a = op == MIN
				? (f32_lt_quiet(F(a), F(b)) || (eq &&  (a & S)))
				: (f32_lt_quiet(F(b), F(a)) || (eq && !(a & S)));
			r = pick_a ? a : b;
		}
		softfloat_exceptionFlags = flags;
		break;
	}
	case EQ:       r = f32_eq(F(a), F(b)); break;
	case LT:       r = f32_lt(F(a), F(b)); break;
	case LE:       r = f32_le(F(a), F(b)); break;
	case CLASS:    r = fclass(a); break;
	case CVT_W_S:  r = (uint32_t)f32_to_i32(F(a), rm, true); break;
	case CVT_WU_S: r = f32_to_ui32(F(a), rm, true); break;
	case CVT_S_W:  r = i32_to_f32((int32_t)a).v; break;
	case CVT_S_WU: r = ui32_to_f32(a).v; break;
	}
	return {r, (uint32_t)softfloat_exceptionFlags & 0x1f};
}

// ----------------------------------------------------------------------------
// DUT driver

static Vhazard3_fpu *dut;
static uint64_t cycles = 0;

static void tick() {
	dut->clk = 0;
	dut->eval();
	dut->clk = 1;
	dut->eval();
	++cycles;
}

static void reset() {
	dut->rst_n = 0;
	dut->op_vld = 0;
	dut->op_kill = 0;
	tick();
	tick();
	dut->rst_n = 1;
	tick();
}

static bool kill_with_next_issue = false;

static void issue(int op, int rm, uint32_t a, uint32_t b, uint32_t c) {
	if (kill_with_next_issue) {
		// Kill the in-flight op and start this one in the same cycle
		dut->op_kill = 1;
		kill_with_next_issue = false;
	} else if (!dut->op_rdy) {
		fprintf(stderr, "op_rdy low when idle\n");
		exit(2);
	}
	dut->op = op;
	dut->rm = rm;
	dut->op_a = a;
	dut->op_b = b;
	dut->op_c = c;
	dut->op_vld = 1;
	tick();
	dut->op_vld = 0;
	dut->op_kill = 0;
	dut->op = 0x1f;  // garbage while busy: must be ignored
	dut->op_a = dut->op_b = dut->op_c = 0xdeadbeef;
}

static Res run(int op, int rm, uint32_t a, uint32_t b, uint32_t c) {
	issue(op, rm, a, b, c);
	int n = 0;
	while (!dut->result_vld) {
		tick();
		if (++n > 200) {
			fprintf(stderr, "timeout: op=%s\n", op_names[op]);
			exit(2);
		}
	}
	return {dut->result, dut->fflags};
}

// ----------------------------------------------------------------------------
// Checking

static uint64_t n_tests = 0, n_fail = 0;
static uint64_t per_op[NUM_OPS] = {0};

static void check(int op, int rm, uint32_t a, uint32_t b, uint32_t c) {
	if (!uses_rm(op))
		rm = 0;
	Res exp = model(op, rm, a, b, c);
	Res got = run(op, rm, a, b, c);
	++n_tests;
	++per_op[op];
	if (exp.v != got.v || exp.flags != got.flags) {
		++n_fail;
		if (n_fail <= 25) {
			printf("MISMATCH %-8s rm=%d a=%08x b=%08x c=%08x  expect %08x/%02x  got %08x/%02x\n",
				op_names[op], rm, a, b, c, exp.v, exp.flags, got.v, got.flags);
		}
	}
}

// ----------------------------------------------------------------------------
// Input generation: biased towards the classes where FPUs go wrong

static std::mt19937_64 rng;

static uint32_t rnd32() { return (uint32_t)rng(); }
static uint32_t rnd_range(uint32_t lo, uint32_t hi) { return lo + rnd32() % (hi - lo + 1); }

static uint32_t mant_pattern() {
	switch (rnd32() % 6) {
	case 0:  return 0;
	case 1:  return 0x7fffff;
	case 2:  return 1u << (rnd32() % 23);
	case 3:  return 0x7fffff ^ (1u << (rnd32() % 23));
	case 4:  return (0x7fffffu >> (rnd32() % 23));
	default: return (0x7fffffu << (rnd32() % 23)) & 0x7fffff;
	}
}

static uint32_t rnd_f32() {
	uint32_t s = (rnd32() & 1) << 31;
	uint32_t f = rnd32() & 0x7fffff;
	uint32_t w = rnd32() % 100;
	if (w < 5)  return s;                                   // +-0
	if (w < 13) return s | (f ? f : 1);                     // subnormal
	if (w < 16) return s | 0x7f800000;                      // inf
	if (w < 19) return s | 0x7fc00000 | (f & 0x3fffff);     // qNaN
	if (w < 22) return s | 0x7f800000 | ((f & 0x3fffff) ? (f & 0x3fffff) : 1); // sNaN
	if (w < 50) return s | rnd_range(1, 254) << 23 | f;     // any normal
	if (w < 72) return s | rnd_range(117, 137) << 23 | f;   // near 1.0
	if (w < 80) return s | (rnd32() & 1 ? rnd_range(1, 6) : rnd_range(249, 254)) << 23 | f;
	if (w < 90) return s | rnd_range(0, 254) << 23 | mant_pattern();
	return rnd32();
}

static uint32_t nudge(uint32_t x) {
	// move a few ulps either way (bit-pattern arithmetic)
	int d = (int)(rnd32() % 7) - 3;
	return x + (uint32_t)d;
}

static uint32_t rnd_int() {
	switch (rnd32() % 8) {
	case 0: {
		static const uint32_t edge[] = {0, 1, 0xffffffffu, 0x80000000u, 0x7fffffffu,
			0x00ffffffu, 0x01000000u, 0x01000001u, 0xff000000u, 0x80000001u};
		return edge[rnd32() % (sizeof(edge) / sizeof(edge[0]))];
	}
	case 1:  return rnd32();
	default: {
		uint32_t bits = rnd32() % 33;
		uint32_t v = bits ? rnd32() >> (32 - bits) : 0;
		return rnd32() & 1 ? -v : v;
	}
	}
}

static uint32_t rnd_cvt_src() {
	// floats near the integer range boundaries and around 0.5 fractions
	if (rnd32() % 3 == 0)
		return rnd_f32();
	uint32_t s = (rnd32() & 1) << 31;
	uint32_t e = rnd_range(127 - 3, 127 + 33);
	uint32_t f = rnd32() % 4 == 0 ? mant_pattern() : rnd32() & 0x7fffff;
	return s | e << 23 | f;
}

// A result value right at an exponent-range boundary: around the smallest
// normal (tininess before/after rounding), largest subnormal, or largest finite.
static uint32_t boundary_target() {
	static const uint32_t t[] = {0x00800000u, 0x007fffffu, 0x00800001u, 0x00000001u, 0x7f7fffffu};
	return nudge(t[rnd32() % 5]) | ((rnd32() & 1) << 31);
}

static void gen_and_check(int op) {
	int rm = rnd32() % 5;
	uint32_t a = rnd_f32(), b = rnd_f32(), c = rnd_f32();
	if (rnd32() % 6 == 0 && (op == MUL || op == DIV || (op >= MADD && op <= NMADD))) {
		// Steer the exact result to an exponent-range boundary, with a
		// slightly perturbed operand so the rounding bits vary.
		softfloat_roundingMode = rnd32() % 5;
		a = (rnd32() & 0x807fffffu) | rnd_range(100, 154) << 23;
		uint32_t t = boundary_target();
		b = nudge(op == DIV ? f32_div(F(a), F(t)).v : f32_div(F(t), F(a)).v);
		if (op != DIV && op != MUL)
			c = rnd32() & 1 ? (rnd32() & 0x80000000u) : (rnd32() & 0x800003ffu);
		check(op, rm, a, b, c);
		return;
	}
	switch (op) {
	case ADD: case SUB:
		if (rnd32() % 4 == 0) {
			b = nudge(op == ADD ? a ^ 0x80000000u : a);   // cancellation
		} else if (rnd32() % 4 == 0) {
			b = (b & 0x807fffffu) | (((a >> 23) & 0xff) + rnd32() % 5 - 2) << 23 % 256;
		}
		break;
	case MADD: case MSUB: case NMSUB: case NMADD:
		if (rnd32() % 4 == 0) {
			softfloat_roundingMode = rnd32() % 5;
			uint32_t p = f32_mul(F(a), F(b)).v;
			bool want_neg = op == MADD || op == NMADD;    // make c cancel the product
			bool flip = (op == NMSUB || op == NMADD);
			c = nudge(p ^ ((want_neg != flip) ? 0x80000000u : 0));
		}
		break;
	case CVT_W_S: case CVT_WU_S:
		a = rnd_cvt_src();
		break;
	case CVT_S_W: case CVT_S_WU:
		a = rnd_int();
		break;
	case EQ: case LT: case LE: case MIN: case MAX:
		if (rnd32() % 4 == 0)
			b = rnd32() & 1 ? a : nudge(a);
		break;
	default:
		break;
	}
	check(op, rm, a, b, c);
}

// ----------------------------------------------------------------------------

static int finish() {
	printf("%llu vectors, %llu cycles, %llu mismatches: %s\n",
		(unsigned long long)n_tests, (unsigned long long)cycles,
		(unsigned long long)n_fail, n_fail ? "FAIL" : "PASS");
	return n_fail ? 1 : 0;
}

int main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	if (argc < 4) {
		fprintf(stderr, "usage: %s random <op|all> <count> [seed]\n"
		                "       %s stdin <op> <rm>\n", argv[0], argv[0]);
		return 2;
	}
	dut = new Vhazard3_fpu;
	reset();

	std::string mode = argv[1];
	if (mode == "stdin") {
		int op = parse_op(argv[2]);
		int rm = atoi(argv[3]);
		if (op < 0) { fprintf(stderr, "unknown op %s\n", argv[2]); return 2; }
		char line[256];
		while (fgets(line, sizeof(line), stdin)) {
			uint32_t v[3] = {0, 0, 0};
			char *p = line;
			for (int i = 0; i < num_srcs(op); ++i)
				v[i] = (uint32_t)strtoul(p, &p, 16);
			check(op, rm, v[0], v[1], v[2]);
		}
		return finish();
	}

	int op = strcmp(argv[2], "all") ? parse_op(argv[2]) : -1;
	if (op < 0 && strcmp(argv[2], "all")) { fprintf(stderr, "unknown op %s\n", argv[2]); return 2; }
	uint64_t count = strtoull(argv[3], nullptr, 0);
	rng.seed(argc > 4 ? strtoull(argv[4], nullptr, 0) : 1);

	for (uint64_t i = 0; i < count; ++i) {
		int this_op = op >= 0 ? op : (int)(rnd32() % NUM_OPS);
		// Occasionally abandon an operation part-way through (op_kill)
		if (rnd32() % 64 == 0) {
			issue(ADD + rnd32() % (SQRT + 1), rnd32() % 5, rnd_f32(), rnd_f32(), rnd_f32());
			for (uint32_t k = rnd32() % 30; k; --k)
				tick();
			if (rnd32() & 1) {
				dut->op_kill = 1;          // kill alone
				tick();
				dut->op_kill = 0;
			} else {
				kill_with_next_issue = true; // kill + new op together
			}
		}
		gen_and_check(this_op);
	}
	if (op < 0) {
		for (int i = 0; i < NUM_OPS; ++i)
			printf("%s=%llu ", op_names[i], (unsigned long long)per_op[i]);
		printf("\n");
	}
	return finish();
}

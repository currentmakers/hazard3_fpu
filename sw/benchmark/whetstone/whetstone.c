// Whetstone benchmark, single precision, for Hazard3 on the Colorlight i5.
//
// The classic Curnow & Wichmann (1976) synthetic benchmark, following the
// module structure and loop counts of the common C translation: LOOP = 10
// gives one million Whetstone instructions per pass. Modules 1, 5 and 10 have
// zero iterations in the original and are omitted. float replaces double, as
// the FPU is single precision.
//
// Built twice from this source (see Makefile): with Zfinx, the float
// arithmetic runs on the FPU; without it, on libgcc soft-float. Both builds
// and a host build use only correctly rounded IEEE operations in the same
// order (-ffp-contract=off), so all three must produce bit-identical module
// results; the target builds check theirs against the host's.
//
//   whetstone_host           run on the build machine
//   whetstone_host --ref     print the reference header for the target builds
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "fmath.h"

#define LOOP       10          // 1 MWI (million Whetstone instructions) per pass
#define MIN_SECS   2           // run at least this long

// ----------------------------------------------------------------------------
// Platform

#ifdef HOST
#include <stdio.h>
#include <string.h>
#include <time.h>
#define TICK_HZ 1000000000u   // "cycles" are nanoseconds on the host
static void out(const char *s) { fputs(s, stdout); }
static uint64_t ticks(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}
#else
#include "uart.h"
#include "whetstone_ref.h"
#define TICK_HZ CLK_HZ
static void out(const char *s) { uart_puts(s); }
static uint64_t ticks(void) {
	uint32_t hi, lo, hi2;
	do {
		__asm__ volatile ("csrr %0, mcycleh" : "=r" (hi));
		__asm__ volatile ("csrr %0, mcycle"  : "=r" (lo));
		__asm__ volatile ("csrr %0, mcycleh" : "=r" (hi2));
	} while (hi != hi2);
	return (uint64_t)hi << 32 | lo;
}
void trap_handler(uint32_t mcause, uint32_t mepc) {
	out("\nTRAP mcause=");
	uart_puthex(mcause);
	out(" mepc=");
	uart_puthex(mepc);
	out("\n");
	for (;;)
		;
}
#endif

static void out_u64(uint64_t x) {
	char buf[21];
	int i = 20;
	buf[i] = 0;
	do {
		buf[--i] = '0' + (char)(x % 10);
		x /= 10;
	} while (x);
	out(&buf[i]);
}

// x / 10^decimals, printed with that many decimals, right-aligned in width
static void out_fixed(uint64_t x, int decimals, int width) {
	char buf[24];
	int i = 23;
	buf[i] = 0;
	for (int d = 0; d < decimals; ++d) {
		buf[--i] = '0' + (char)(x % 10);
		x /= 10;
	}
	if (decimals)
		buf[--i] = '.';
	do {
		buf[--i] = '0' + (char)(x % 10);
		x /= 10;
	} while (x);
	while (23 - i < width)
		buf[--i] = ' ';
	out(&buf[i]);
}

static void out_hex(uint32_t x) {
	char buf[9];
	for (int i = 7; i >= 0; --i, x >>= 4)
		buf[i] = "0123456789abcdef"[x & 0xf];
	buf[8] = 0;
	out(buf);
}

static uint32_t fbits(float f) {
	union { float f; uint32_t u; } b = { .f = f };
	return b.u;
}

// ----------------------------------------------------------------------------
// Whetstone

static float T, T1, T2, E1[5];
static int J, K, L;

static void __attribute__((noinline)) PA(float E[5]) {
	J = 0;
	do {
		E[1] = ( E[1] + E[2] + E[3] - E[4]) * T;
		E[2] = ( E[1] + E[2] - E[3] + E[4]) * T;
		E[3] = ( E[1] - E[2] + E[3] + E[4]) * T;
		E[4] = (-E[1] + E[2] + E[3] + E[4]) / T2;
		J += 1;
	} while (J < 6);
}

static void __attribute__((noinline)) P0(void) {
	E1[J] = E1[K];
	E1[K] = E1[L];
	E1[L] = E1[J];
}

static void __attribute__((noinline)) P3(float X, float Y, float *Z) {
	float X1 = X, Y1 = Y;
	X1 = T * (X1 + Y1);
	Y1 = T * (X1 + Y1);
	*Z = (X1 + Y1) / T2;
}

enum { M2, M3, M4, M6, M7, M8, M9, M11, N_MOD };

static const char *const mod_name[N_MOD] = {
	" 2 array elements    ",
	" 3 array as param    ",
	" 4 conditional jumps ",
	" 6 integer arith     ",
	" 7 trig functions    ",
	" 8 procedure calls   ",
	" 9 array references  ",
	"11 standard functions",
};

// Values left by each module in the last pass (bit patterns), checked against
// the host reference
static uint32_t result[N_MOD][4];
static uint64_t mod_ticks[N_MOD];

static void whetstone(int passes) {
	const int N2 = 12 * LOOP, N3 = 14 * LOOP, N4 = 345 * LOOP, N6 = 210 * LOOP,
	          N7 = 32 * LOOP, N8 = 899 * LOOP, N9 = 616 * LOOP, N11 = 93 * LOOP;
	float X, Y, Z;
	uint64_t t;

	T = 0.499975f;
	T1 = 0.50025f;
	T2 = 2.0f;
	for (int m = 0; m < N_MOD; ++m)
		mod_ticks[m] = 0;

	for (int pass = 0; pass < passes; ++pass) {
		// Module 2: array elements
		t = ticks();
		E1[1] = 1.0f; E1[2] = -1.0f; E1[3] = -1.0f; E1[4] = -1.0f;
		for (int i = 0; i < N2; ++i) {
			E1[1] = ( E1[1] + E1[2] + E1[3] - E1[4]) * T;
			E1[2] = ( E1[1] + E1[2] - E1[3] + E1[4]) * T;
			E1[3] = ( E1[1] - E1[2] + E1[3] + E1[4]) * T;
			E1[4] = (-E1[1] + E1[2] + E1[3] + E1[4]) * T;
		}
		mod_ticks[M2] += ticks() - t;
		result[M2][0] = fbits(E1[1]); result[M2][1] = fbits(E1[2]);
		result[M2][2] = fbits(E1[3]); result[M2][3] = fbits(E1[4]);

		// Module 3: array as parameter
		t = ticks();
		for (int i = 0; i < N3; ++i)
			PA(E1);
		mod_ticks[M3] += ticks() - t;
		result[M3][0] = fbits(E1[1]); result[M3][1] = fbits(E1[2]);
		result[M3][2] = fbits(E1[3]); result[M3][3] = fbits(E1[4]);

		// Module 4: conditional jumps
		t = ticks();
		J = 1;
		for (int i = 0; i < N4; ++i) {
			J = J == 1 ? 2 : 3;
			J = J > 2 ? 0 : 1;
			J = J < 1 ? 1 : 0;
		}
		mod_ticks[M4] += ticks() - t;
		result[M4][0] = (uint32_t)J;

		// Module 6: integer arithmetic
		t = ticks();
		J = 1; K = 2; L = 3;
		for (int i = 0; i < N6; ++i) {
			J = J * (K - J) * (L - K);
			K = L * K - (L - J) * K;
			L = (L - K) * (K + J);
			E1[L - 1] = (float)(J + K + L);
			E1[K - 1] = (float)(J * K * L);
		}
		mod_ticks[M6] += ticks() - t;
		result[M6][0] = (uint32_t)J; result[M6][1] = (uint32_t)K;
		result[M6][2] = fbits(E1[1]); result[M6][3] = fbits(E1[2]);

		// Module 7: trigonometric functions
		t = ticks();
		X = 0.5f; Y = 0.5f;
		for (int i = 0; i < N7; ++i) {
			X = T * f_atan(T2 * f_sin(X) * f_cos(X) / (f_cos(X + Y) + f_cos(X - Y) - 1.0f));
			Y = T * f_atan(T2 * f_sin(Y) * f_cos(Y) / (f_cos(X + Y) + f_cos(X - Y) - 1.0f));
		}
		mod_ticks[M7] += ticks() - t;
		result[M7][0] = fbits(X); result[M7][1] = fbits(Y);

		// Module 8: procedure calls
		t = ticks();
		X = 1.0f; Y = 1.0f; Z = 1.0f;
		for (int i = 0; i < N8; ++i)
			P3(X, Y, &Z);
		mod_ticks[M8] += ticks() - t;
		result[M8][0] = fbits(Z);

		// Module 9: array references
		t = ticks();
		J = 1; K = 2; L = 3;
		E1[1] = 1.0f; E1[2] = 2.0f; E1[3] = 3.0f;
		for (int i = 0; i < N9; ++i)
			P0();
		mod_ticks[M9] += ticks() - t;
		result[M9][0] = fbits(E1[1]); result[M9][1] = fbits(E1[2]);
		result[M9][2] = fbits(E1[3]);

		// Module 11: standard functions
		t = ticks();
		X = 0.75f;
		for (int i = 0; i < N11; ++i)
			X = f_sqrt(f_exp(f_log(X) / T1));
		mod_ticks[M11] += ticks() - t;
		result[M11][0] = fbits(X);
	}
}

// ----------------------------------------------------------------------------

int main(int argc, char **argv) {
#ifdef HOST
	if (argc > 1 && !strcmp(argv[1], "--ref")) {
		whetstone(1);
		printf("// Whetstone module results from the host build (generated, do not edit)\n");
		printf("static const uint32_t whetstone_ref[%d][4] = {\n", N_MOD);
		for (int m = 0; m < N_MOD; ++m)
			printf("\t{0x%08x, 0x%08x, 0x%08x, 0x%08x},\n",
				result[m][0], result[m][1], result[m][2], result[m][3]);
		printf("};\n");
		return 0;
	}
	out("\nWhetstone (single precision), host build\n");
#else
	(void)argc;
	(void)argv;
	uart_init(115200);
	__asm__ volatile ("csrw mcountinhibit, zero");   // Hazard3 resets with counters off
	// The DAPLink sometimes drops UART data while JTAG traffic is still going
	// on (OpenOCD finishing after "resume"), so wait 0.3 s before printing
	for (uint64_t t0 = ticks(); ticks() - t0 < CLK_HZ / 10 * 3;)
		;
#if defined(__riscv_zfinx)
	out("\nWhetstone (single precision), Hazard3 @ ");
	out_u64(CLK_HZ / 1000000);
	out(" MHz, FPU: Zfinx hardware\n");
#else
	out("\nWhetstone (single precision), Hazard3 @ ");
	out_u64(CLK_HZ / 1000000);
	out(" MHz, FPU: none (libgcc soft-float)\n");
#endif
#endif

	// Calibrate: one pass, then enough passes for MIN_SECS
	whetstone(1);
	uint64_t one = 0;
	for (int m = 0; m < N_MOD; ++m)
		one += mod_ticks[m];
	uint64_t passes = (uint64_t)MIN_SECS * TICK_HZ / one + 1;
	out("Running ");
	out_u64(passes);
	out(" passes of 1 MWI...\n");

	whetstone((int)passes);
	uint64_t total = 0;
	for (int m = 0; m < N_MOD; ++m)
		total += mod_ticks[m];

	out("\nModule                     share   result\n");
	int bad = 0;
	for (int m = 0; m < N_MOD; ++m) {
		out(mod_name[m]);
		out_fixed(mod_ticks[m] * 1000 / total, 1, 7);
		out("%   ");
		for (int i = 0; i < 4; ++i) {
			out_hex(result[m][i]);
			out(" ");
		}
#ifndef HOST
		for (int i = 0; i < 4; ++i)
			if (result[m][i] != whetstone_ref[m][i]) {
				bad = 1;
				out(" MISMATCH");
				break;
			}
#endif
		out("\n");
	}

	// MWIPS = passes * 1 MWI / seconds
	uint64_t mwips_milli = passes * 1000u * TICK_HZ / total;
	out("\nTime:   ");
	out_fixed(total / (TICK_HZ / 1000u), 3, 0);
	out(" s\nMWIPS:  ");
	out_fixed(mwips_milli, 3, 0);
#ifndef HOST
	out("\nMWIPS/MHz: ");
	out_fixed(passes * 10000000000ull / total, 4, 0);  // passes * 1e6 / cycles, 4 decimals
	out(bad ? "\nResults: MISMATCH against host reference: FAIL\n"
	        : "\nResults: bit-identical to host reference: PASS\n");
#else
	out("\n");
#endif
	out("Done.\n");
	return bad;
}

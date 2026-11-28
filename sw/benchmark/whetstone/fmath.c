// Small single-precision math library for the Whetstone benchmark (see fmath.h).
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "fmath.h"

typedef union { float f; uint32_t u; } fbits_t;

static inline uint32_t f2u(float f) { fbits_t b; b.f = f; return b.u; }
static inline float u2f(uint32_t u) { fbits_t b; b.u = u; return b.f; }

// Round to nearest integer (ties away from zero), for argument reduction
static inline int nearest(float x) { return (int)(x + (x < 0.0f ? -0.5f : 0.5f)); }

// pi/2 and ln2 split into parts with 12 trailing zero bits (k * part is
// exact for |k| < 4096) and a rounded remainder.
#define PIO2_1      1.5703125f
#define PIO2_2      4.837512969970703e-04f
#define PIO2_3      7.549790126404332e-08f
#define TWO_OVER_PI 0.6366197466850281f
#define LN2_HI      0.693115234375f
#define LN2_LO      3.194618329871446e-05f
#define INV_LN2     1.4426950216293335f

// Polynomials on |r| <= pi/4 (Taylor; truncation error < 3e-9)
static inline float sin_poly(float r) {
	float z = r * r;
	return r + r * z * (-1.0f / 6 + z * (1.0f / 120 + z * (-1.0f / 5040 + z * (1.0f / 362880))));
}

static inline float cos_poly(float r) {
	float z = r * r;
	return 1.0f + z * (-0.5f + z * (1.0f / 24 + z * (-1.0f / 720 + z * (1.0f / 40320 + z * (-1.0f / 3628800)))));
}

static inline float reduce_pio2(float x, int *quadrant) {
	int k = nearest(x * TWO_OVER_PI);
	*quadrant = k & 3;
	float fk = (float)k;
	return ((x - fk * PIO2_1) - fk * PIO2_2) - fk * PIO2_3;
}

float f_sin(float x) {
	int q;
	float r = reduce_pio2(x, &q);
	switch (q) {
	case 0:  return  sin_poly(r);
	case 1:  return  cos_poly(r);
	case 2:  return -sin_poly(r);
	default: return -cos_poly(r);
	}
}

float f_cos(float x) {
	int q;
	float r = reduce_pio2(x, &q);
	switch (q) {
	case 0:  return  cos_poly(r);
	case 1:  return -sin_poly(r);
	case 2:  return -cos_poly(r);
	default: return  sin_poly(r);
	}
}

float f_atan(float x) {
	int neg = x < 0.0f, inv = 0;
	float a = neg ? -x : x, off = 0.0f;
	if (a > 1.0f) {                    // atan(a) = pi/2 - atan(1/a)
		a = 1.0f / a;
		inv = 1;
	}
	if (a > 0.2679491937160492f) {     // > tan(pi/12): atan(a) = pi/6 + atan((a*sqrt3-1)/(a+sqrt3))
		a = (a * 1.7320507764816284f - 1.0f) / (a + 1.7320507764816284f);
		off = 0.5235987901687622f;
	}
	float z = a * a;                   // |a| <= tan(pi/12): Taylor, error < 3e-9
	float r = a + a * z * (-1.0f / 3 + z * (1.0f / 5 + z * (-1.0f / 7 + z * (1.0f / 9 + z * (-1.0f / 11)))));
	r += off;
	if (inv)
		r = 1.5707963705062866f - r;
	return neg ? -r : r;
}

float f_exp(float x) {
	if (x != x)
		return x;
	if (x > 88.8f)
		return u2f(0x7f800000u);       // +inf
	if (x < -87.3f)
		return 0.0f;                   // (no subnormal results)
	int k = nearest(x * INV_LN2);
	float r = (x - (float)k * LN2_HI) - (float)k * LN2_LO;   // |r| <= ln2/2
	float p = 1.0f + r * (1.0f + r * (0.5f + r * (1.0f / 6 + r * (1.0f / 24 + r * (1.0f / 120 + r * (1.0f / 720 + r * (1.0f / 5040)))))));
	if (k > 127) {                     // 2^k does not fit in one float
		p *= 2.0f;
		k--;
	}
	return p * u2f((uint32_t)(k + 127) << 23);
}

float f_log(float x) {
	uint32_t u = f2u(x);
	if (x != x || u == 0x7f800000u)
		return x;                      // NaN, +inf
	if (x == 0.0f)
		return u2f(0xff800000u);       // -inf
	if (x < 0.0f)
		return u2f(0x7fc00000u);       // NaN
	int e = 0;
	if (!(u & 0x7f800000u)) {          // subnormal: scale by 2^25
		u = f2u(x * 33554432.0f);
		e = -25;
	}
	e += (int)(u >> 23) - 127;
	float m = u2f((u & 0x7fffffu) | 0x3f800000u);   // [1, 2)
	if (m > 1.4142135381698608f) {
		m *= 0.5f;
		e++;
	}
	float s = (m - 1.0f) / (m + 1.0f);              // |s| <= 0.1716
	float z = s * s;
	float lm = 2.0f * s + 2.0f * s * z * (1.0f / 3 + z * (1.0f / 5 + z * (1.0f / 7 + z * (1.0f / 9))));
	return (float)e * LN2_HI + (lm + (float)e * LN2_LO);
}

float f_sqrt(float x) {
#if defined(__riscv_zfinx)
	float r;
	__asm__ ("fsqrt.s %0, %1" : "=r" (r) : "r" (x));
	return r;
#else
	// Correctly rounded square root with integer arithmetic (what a
	// soft-float library has to do)
	uint32_t u = f2u(x);
	if (x != x || u == 0x7f800000u || x == 0.0f)
		return x;                      // NaN, +inf, +-0
	if (u >> 31)
		return u2f(0x7fc00000u);       // negative: NaN
	int e = (int)(u >> 23) & 0xff;
	uint32_t m = u & 0x7fffffu;
	if (e == 0) {                      // subnormal: normalise
		while (!(m & 0x800000u)) {
			m <<= 1;
			e--;
		}
		e++;
	} else {
		m |= 0x800000u;
	}
	int ex = e - 127;                  // x = m * 2^(ex - 23), m in [2^23, 2^24)
	if (ex & 1) {
		m <<= 1;
		ex--;
	}
	// sqrt(m * 2^25) has 25 bits: 24 result bits plus a round bit. The
	// remainder is never zero with the round bit set, so no ties.
	uint64_t rem = (uint64_t)m << 25, root = 0, bit = 1ull << 48;
	while (bit) {
		if (rem >= root + bit) {
			rem -= root + bit;
			root = (root >> 1) + bit;
		} else {
			root >>= 1;
		}
		bit >>= 2;
	}
	uint32_t r = (uint32_t)root;
	r = (r >> 1) + (r & 1);            // round to nearest
	if (r >> 24) {                     // rounded up to 2.0
		r >>= 1;
		ex += 2;
	}
	return u2f((uint32_t)(ex / 2 + 127) << 23 | (r & 0x7fffffu));
#endif
}

// On-board FPU self-test for Hazard3 + Zfinx on the Colorlight i5.
//
// Runs SoftFloat-generated golden vectors through every Zfinx instruction
// (rounding mode via frm, exception flags checked via fflags), prints any
// mismatches, then measures the cycle count of a few common operations.
// Regenerate the vectors with: make -C ../../verif/zfinx OUT=$PWD/zfinx_vectors.h N=1000 SEED=7

#include <stdint.h>
#include "uart.h"
#include "zfinx_vectors.h"

enum {
	ADD, SUB, MUL, MADD, MSUB, NMSUB, NMADD, DIV, SQRT, SGNJ, SGNJN, SGNJX,
	MIN, MAX, EQ, LT, LE, CLASS, CVT_W_S, CVT_WU_S, CVT_S_W, CVT_S_WU
};

#define OP1(insn) __asm__ volatile (insn " %0, %1"         : "=r" (r) : "r" (a))
#define OP2(insn) __asm__ volatile (insn " %0, %1, %2"     : "=r" (r) : "r" (a), "r" (b))
#define OP3(insn) __asm__ volatile (insn " %0, %1, %2, %3" : "=r" (r) : "r" (a), "r" (b), "r" (c))

static uint32_t __attribute__((noinline)) exec(int op, uint32_t a, uint32_t b, uint32_t c) {
	uint32_t r = 0;
	switch (op) {
	case ADD:      OP2("fadd.s");    break;
	case SUB:      OP2("fsub.s");    break;
	case MUL:      OP2("fmul.s");    break;
	case MADD:     OP3("fmadd.s");   break;
	case MSUB:     OP3("fmsub.s");   break;
	case NMSUB:    OP3("fnmsub.s");  break;
	case NMADD:    OP3("fnmadd.s");  break;
	case DIV:      OP2("fdiv.s");    break;
	case SQRT:     OP1("fsqrt.s");   break;
	case SGNJ:     OP2("fsgnj.s");   break;
	case SGNJN:    OP2("fsgnjn.s");  break;
	case SGNJX:    OP2("fsgnjx.s");  break;
	case MIN:      OP2("fmin.s");    break;
	case MAX:      OP2("fmax.s");    break;
	case EQ:       OP2("feq.s");     break;
	case LT:       OP2("flt.s");     break;
	case LE:       OP2("fle.s");     break;
	case CLASS:    OP1("fclass.s");  break;
	case CVT_W_S:  OP1("fcvt.w.s");  break;
	case CVT_WU_S: OP1("fcvt.wu.s"); break;
	case CVT_S_W:  OP1("fcvt.s.w");  break;
	case CVT_S_WU: OP1("fcvt.s.wu"); break;
	}
	return r;
}

static inline uint32_t rdcycle(void) {
	uint32_t x;
	__asm__ volatile ("csrr %0, mcycle" : "=r" (x));
	return x;
}

static void put_dec(uint32_t x) {
	char buf[11];
	int i = 10;
	buf[i] = 0;
	do {
		buf[--i] = '0' + x % 10;
		x /= 10;
	} while (x);
	uart_puts(&buf[i]);
}

// Cycles taken by one instruction, minus the cost of the empty measurement
#define TIME_OP(name, insn) do {                                      \
	uint32_t t0, t1, r;                                                \
	__asm__ volatile (                                                 \
		"csrr %0, mcycle\n" insn " %2, %3, %4\n csrr %1, mcycle"       \
		: "=&r" (t0), "=&r" (t1), "=&r" (r) : "r" (x), "r" (y));       \
	uart_puts("  " name ": ");                                         \
	put_dec(t1 - t0 - overhead);                                       \
	uart_puts(" cycles\n");                                            \
} while (0)

void trap_handler(uint32_t mcause, uint32_t mepc) {
	uart_puts("\nTRAP mcause=");
	uart_puthex(mcause);
	uart_puts(" mepc=");
	uart_puthex(mepc);
	uart_puts("\n");
	for (;;)
		;
}

int main(void) {
	uart_init(115200);
	// Hazard3 resets with the counters inhibited
	__asm__ volatile ("csrw mcountinhibit, zero");
	uart_puts("\nHazard3 Zfinx FPU self-test\n");

	int mismatches = 0;
	uint32_t t_start = rdcycle();
	for (int i = 0; i < ZFINX_N_VECS; ++i) {
		const zfinx_vec_t *v = &zfinx_vecs[i];
		uint32_t flags;
		__asm__ volatile ("csrw frm, %0\n csrw fflags, zero" : : "r" ((uint32_t)v->rm));
		uint32_t r = exec(v->op, v->a, v->b, v->c);
		__asm__ volatile ("csrr %0, fflags" : "=r" (flags));
		if (r != v->result || flags != v->flags) {
			if (++mismatches <= 10) {
				uart_puts("  MISMATCH vec ");
				put_dec(i);
				uart_puts(": got ");
				uart_puthex(r);
				uart_puts("/");
				uart_puthex(flags);
				uart_puts(" expected ");
				uart_puthex(v->result);
				uart_puts("/");
				uart_puthex(v->flags);
				uart_puts("\n");
			}
		}
	}
	uint32_t t_total = rdcycle() - t_start;
	__asm__ volatile ("csrw fcsr, zero");

	put_dec(ZFINX_N_VECS);
	uart_puts(" vectors, ");
	put_dec(mismatches);
	uart_puts(mismatches ? " mismatches: FAIL\n" : " mismatches: PASS\n");
	uart_puts("  (");
	put_dec(t_total);
	uart_puts(" cycles in total)\n");

	// Per-instruction latency with normal operands (pi, e)
	uint32_t x = 0x40490fdb, y = 0x402df854, overhead, t0, t1;
	__asm__ volatile ("csrr %0, mcycle\n csrr %1, mcycle" : "=&r" (t0), "=&r" (t1));
	overhead = t1 - t0;
	uart_puts("Latency:\n");
	TIME_OP("fadd.s ", "fadd.s");
	TIME_OP("fmul.s ", "fmul.s");
	TIME_OP("fdiv.s ", "fdiv.s");
	TIME_OP("fmin.s ", "fmin.s");
	TIME_OP("feq.s  ", "feq.s");
	{
		uint32_t r;
		__asm__ volatile ("csrr %0, mcycle\n fmadd.s %2, %3, %4, %3\n csrr %1, mcycle"
			: "=&r" (t0), "=&r" (t1), "=&r" (r) : "r" (x), "r" (y));
		uart_puts("  fmadd.s: ");
		put_dec(t1 - t0 - overhead);
		uart_puts(" cycles\n");
		__asm__ volatile ("csrr %0, mcycle\n fsqrt.s %2, %3\n csrr %1, mcycle"
			: "=&r" (t0), "=&r" (t1), "=&r" (r) : "r" (x));
		uart_puts("  fsqrt.s: ");
		put_dec(t1 - t0 - overhead);
		uart_puts(" cycles\n");
	}

	uart_puts("Done. Echoing UART input...\n");
	for (;;) {
		if (!(UART_FSTAT & UART_FSTAT_RXEMPTY))
			uart_putc((char)UART_RX);
	}
}

// Dhrystone 2.x on Hazard3 (Colorlight i5): board glue.
//
// The benchmark itself is Hazard3's copy of the riscv-tests Dhrystone
// (hazard3/test/sim/dhrystone/src), compiled unmodified with its main()
// renamed to dhrystone_main() (dhrystone_main_wrap.c). Its timer is mcycle with "HZ" = 1000000, so
// the "Microseconds" it prints are really cycles per run and its "per Second"
// figure is per million cycles; the summary below converts to real units.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdarg.h>
#include "uart.h"
#include "tiny_printf.h"

int dhrystone_main(int argc, char **argv);

// Dhrystone globals, for the result check and the summary
extern long User_Time;
extern int Int_Glob, Bool_Glob, Arr_1_Glob[50], Arr_2_Glob[50][50];
extern char Ch_1_Glob, Ch_2_Glob;

void debug_printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	tiny_vprintf(fmt, ap);
	va_end(ap);
}

void trap_handler(uint32_t mcause, uint32_t mepc) {
	tiny_printf("\nTRAP mcause=%x mepc=%x\n", (unsigned)mcause, (unsigned)mepc);
	for (;;)
		;
}

static uint32_t rdcycle(void) {
	uint32_t x;
	__asm__ volatile ("csrr %0, mcycle" : "=r" (x));
	return x;
}

int main(void) {
	uart_init(115200);
	__asm__ volatile ("csrw mcountinhibit, zero");   // counters are off at reset
	// The DAPLink can drop UART data while OpenOCD is still busy after
	// "resume"; wait 0.3 s before printing
	for (uint32_t t0 = rdcycle(); rdcycle() - t0 < CLK_HZ / 10 * 3;)
		;

	dhrystone_main(0, 0);

	// Arr_2_Glob[8][7] ends up as Number_Of_Runs + 10
	uint32_t runs = (uint32_t)Arr_2_Glob[8][7] - 10;
	uint32_t cycles = (uint32_t)User_Time;
	int ok = Int_Glob == 5 && Bool_Glob == 1 && Ch_1_Glob == 'A' && Ch_2_Glob == 'B'
		&& Arr_1_Glob[8] == 7;

	// 1 DMIPS = 1757 Dhrystones per second (VAX 11/780)
	uint64_t dps = (uint64_t)runs * CLK_HZ / cycles;
	uint64_t dmips_mhz_x1000 = (uint64_t)runs * 1000000000ull / cycles / 1757;
	uint64_t dmips_x100 = (uint64_t)runs * CLK_HZ * 100 / cycles / 1757;

	tiny_printf("\nSummary (Hazard3 @ %u MHz)\n", (unsigned)(CLK_HZ / 1000000));
	tiny_printf("Runs:               %u in %u cycles\n", (unsigned)runs, (unsigned)cycles);
	tiny_printf("Cycles per run:     %u.%02u\n", (unsigned)(cycles / runs),
		(unsigned)((uint64_t)(cycles % runs) * 100 / runs));
	tiny_printf("Dhrystones/s:       %u\n", (unsigned)dps);
	tiny_printf("DMIPS:              %u.%02u\n", (unsigned)(dmips_x100 / 100), (unsigned)(dmips_x100 % 100));
	tiny_printf("DMIPS/MHz:          %u.%03u\n", (unsigned)(dmips_mhz_x1000 / 1000),
		(unsigned)(dmips_mhz_x1000 % 1000));
	tiny_printf(ok ? "Results: final values correct: PASS\n" : "Results: final values WRONG: FAIL\n");
	tiny_printf("Done.\n");
	return 0;
}

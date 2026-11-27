// Hello world for Hazard3 on Colorlight i5: prints over UART, then echoes input.
#include <stdint.h>
#include "uart.h"

static inline uint32_t read_misa(void) {
	uint32_t x;
	__asm__ volatile ("csrr %0, misa" : "=r"(x));
	return x;
}

static uint32_t f2u(float f) {
	union { float f; uint32_t u; } v = { .f = f };
	return v.u;
}

volatile float fa = 1.5f, fb = 2.25f, fc = -0.75f;

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
	uart_puts("\nHello from Hazard3 on Colorlight i5\n");
	uart_puts("misa = 0x");
	uart_puthex(read_misa());
	uart_puts("\n");

	// Float sanity check: 1.5 * 2.25 + -0.75 = 2.625 (0x40280000)
	uart_puts("fma  = 0x");
	uart_puthex(f2u(fa * fb + fc));
	uart_puts(" (expect 0x40280000)\n");

	uart_puts("Echoing UART input...\n");
	for (;;) {
		if (!(UART_FSTAT & UART_FSTAT_RXEMPTY))
			uart_putc((char)UART_RX);
	}
}

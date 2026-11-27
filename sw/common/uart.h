// UART driver for libfpga uart_mini in the Hazard3 example SoC.
#pragma once
#include <stdint.h>

#define TIMER_BASE 0x40000000u
#define UART_BASE  0x40004000u

#define UART_CSR   (*(volatile uint32_t *)(UART_BASE + 0x0))
#define UART_DIV   (*(volatile uint32_t *)(UART_BASE + 0x4))
#define UART_FSTAT (*(volatile uint32_t *)(UART_BASE + 0x8))
#define UART_TX    (*(volatile uint32_t *)(UART_BASE + 0xc))
#define UART_RX    (*(volatile uint32_t *)(UART_BASE + 0x10))

#define UART_FSTAT_TXFULL  (1u << 8)
#define UART_FSTAT_RXEMPTY (1u << 25)

#ifndef CLK_HZ
#define CLK_HZ 35000000u
#endif

static inline void uart_init(uint32_t baud) {
	// 8x oversampling, divider is 10.4 fixed point: div = clk / (8 * baud)
	uint32_t div16 = (CLK_HZ * 2u + baud / 2u) / baud; // = 16 * clk / (8 * baud)
	UART_CSR = 0;
	UART_DIV = div16;
	UART_CSR = 1;
}

static inline void uart_putc(char c) {
	while (UART_FSTAT & UART_FSTAT_TXFULL)
		;
	UART_TX = (uint8_t)c;
}

static inline void uart_puts(const char *s) {
	for (; *s; ++s) {
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s);
	}
}

static inline void uart_puthex(uint32_t x) {
	for (int i = 28; i >= 0; i -= 4)
		uart_putc("0123456789abcdef"[(x >> i) & 0xf]);
}

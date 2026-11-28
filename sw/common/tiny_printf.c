// Minimal printf to the UART for bare-metal apps (see tiny_printf.h).
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "tiny_printf.h"
#include "uart.h"

static int out_count;

static void out_char(char c) {
	if (c == '\n')
		uart_putc('\r');
	uart_putc(c);
	out_count++;
}

// Print the digits in buf[0..len) (most significant first) padded to width
static void out_field(const char *prefix, const char *buf, int len, int width, int left, int zero) {
	int plen = 0;
	while (prefix[plen])
		plen++;
	int pad = width - len - plen;
	if (!left && !zero)
		for (; pad > 0; --pad)
			out_char(' ');
	for (int i = 0; i < plen; ++i)
		out_char(prefix[i]);
	if (!left && zero)
		for (; pad > 0; --pad)
			out_char('0');
	for (int i = 0; i < len; ++i)
		out_char(buf[i]);
	for (; pad > 0; --pad)
		out_char(' ');
}

static int fmt_u64(char *buf, uint64_t x, unsigned base, int upper) {
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[24];
	int n = 0;
	do {
		tmp[n++] = digits[x % base];
		x /= base;
	} while (x);
	for (int i = 0; i < n; ++i)
		buf[i] = tmp[n - 1 - i];
	return n;
}

static void out_double(double v, int prec, int width, int left, int zero) {
	char buf[48];
	int n = 0;
	const char *sign = "";
	if (v != v) {
		out_field("", "nan", 3, width, left, 0);
		return;
	}
	if (v < 0) {
		sign = "-";
		v = -v;
	}
	if (v > 1.8e19) {
		out_field(sign, "inf", 3, width, left, 0);
		return;
	}
	if (prec > 9)
		prec = 9;
	uint64_t scale = 1;
	for (int i = 0; i < prec; ++i)
		scale *= 10;
	uint64_t ip = (uint64_t)v;
	uint64_t fp = (uint64_t)((v - (double)ip) * (double)scale + 0.5);
	if (fp >= scale) {
		fp -= scale;
		ip++;
	}
	n = fmt_u64(buf, ip, 10, 0);
	if (prec) {
		buf[n++] = '.';
		char tmp[12];
		int m = fmt_u64(tmp, fp, 10, 0);
		for (int i = m; i < prec; ++i)
			buf[n++] = '0';
		for (int i = 0; i < m; ++i)
			buf[n++] = tmp[i];
	}
	out_field(sign, buf, n, width, left, zero);
}

int tiny_vprintf(const char *fmt, va_list ap) {
	out_count = 0;
	for (; *fmt; ++fmt) {
		if (*fmt != '%') {
			out_char(*fmt);
			continue;
		}
		int left = 0, zero = 0, width = 0, prec = -1, lng = 0;
		for (;; ++fmt) {
			if (fmt[1] == '-')
				left = 1;
			else if (fmt[1] == '0')
				zero = 1;
			else
				break;
		}
		++fmt;
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');
		if (*fmt == '.') {
			prec = 0;
			++fmt;
			while (*fmt >= '0' && *fmt <= '9')
				prec = prec * 10 + (*fmt++ - '0');
		}
		while (*fmt == 'l') {
			lng++;
			++fmt;
		}
		char buf[24];
		int n;
		switch (*fmt) {
		case 'd':
		case 'i': {
			int64_t v = lng >= 2 ? va_arg(ap, long long) : lng ? va_arg(ap, long) : va_arg(ap, int);
			uint64_t u = v < 0 ? -(uint64_t)v : (uint64_t)v;
			n = fmt_u64(buf, u, 10, 0);
			out_field(v < 0 ? "-" : "", buf, n, width, left, zero);
			break;
		}
		case 'u':
		case 'x':
		case 'X': {
			uint64_t u = lng >= 2 ? va_arg(ap, unsigned long long)
			           : lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			n = fmt_u64(buf, u, *fmt == 'u' ? 10 : 16, *fmt == 'X');
			out_field("", buf, n, width, left, zero);
			break;
		}
		case 'p':
			n = fmt_u64(buf, (uintptr_t)va_arg(ap, void *), 16, 0);
			out_field("0x", buf, n, width, left, zero);
			break;
		case 'c':
			buf[0] = (char)va_arg(ap, int);
			out_field("", buf, 1, width, left, 0);
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			int len = 0;
			while (s[len] && (prec < 0 || len < prec))
				len++;
			out_field("", s, len, width, left, 0);
			break;
		}
		case 'f':
			out_double(va_arg(ap, double), prec < 0 ? 6 : prec, width, left, zero);
			break;
		case '%':
			out_char('%');
			break;
		case 0:
			return out_count;
		default:
			out_char('%');
			out_char(*fmt);
			break;
		}
	}
	return out_count;
}

int tiny_printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = tiny_vprintf(fmt, ap);
	va_end(ap);
	return n;
}

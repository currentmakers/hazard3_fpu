// Minimal printf to the UART for bare-metal apps.
// Supports %d %i %u %x %X %c %s %p %% and %f (double; precision, default 6),
// with flags '-' and '0', a field width, and the length modifiers l and ll.
// "\n" is sent as "\r\n".
#pragma once
#include <stdarg.h>

int tiny_vprintf(const char *fmt, va_list ap);
int tiny_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

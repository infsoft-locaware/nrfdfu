#include <stdio.h>
#include <stdarg.h>

#include "log.h"

void __attribute__ ((format (printf, 2, 3)))
log_out(enum loglevel level, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

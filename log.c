#include <stdio.h>
#include <stdarg.h>

#include "log.h"

void __attribute__ ((format (printf, 3, 4)))
log_out(enum loglevel level, bool nl, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vprintf(format, args);
	if (nl)
		printf("\n");
	va_end(args);
}

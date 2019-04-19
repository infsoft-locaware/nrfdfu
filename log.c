#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "conf.h"

void __attribute__ ((format (printf, 3, 4)))
log_out(enum loglevel level, bool nl, const char *format, ...)
{
	va_list args;

	if (conf.debug < level)
		return;

	va_start(args, format);
	vprintf(format, args);
	if (nl || conf.debug > level)
		printf("\n");

	va_end(args);
}

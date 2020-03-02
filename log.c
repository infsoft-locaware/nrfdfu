/*
 * nrfdfu - Nordic DFU Upgrade Utility
 *
 * Copyright (C) 2019 Bruno Randolf (br1@einfach.org)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdio.h>

#include "conf.h"
#include "log.h"

void __attribute__((format(printf, 3, 4)))
log_out(enum loglevel level, bool nl, const char *format, ...)
{
    va_list args;

    if (conf.loglevel < level)
        return;

    va_start(args, format);
    vprintf(format, args);
    if (nl || conf.loglevel > level)
        printf("\n");

    va_end(args);
}

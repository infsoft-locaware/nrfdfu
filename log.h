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

#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

/* these conincide with syslog levels for convenience */
enum loglevel { LL_CRIT = 2, LL_ERR, LL_WARN, LL_NOTICE, LL_INFO, LL_DEBUG };

void __attribute__((format(printf, 3, 4)))
log_out(enum loglevel ll, bool nl, const char* fmt, ...);

#ifndef DEBUG
#define DEBUG 1
#endif

#define LOG_CRIT(...)  log_out(LL_CRIT, true, __VA_ARGS__)
#define LOG_ERR(...)   log_out(LL_ERR, true, __VA_ARGS__)
#define LOG_WARN(...)  log_out(LL_WARN, true, __VA_ARGS__)
#define LOG_NOTI(...)  log_out(LL_NOTICE, true, __VA_ARGS__)
#define LOG_NOTI_(...) log_out(LL_NOTICE, false, __VA_ARGS__)
#define LOG_INF(...)   log_out(LL_INFO, true, __VA_ARGS__)
#define LOG_INF_(...)  log_out(LL_INFO, false, __VA_ARGS__)
#define LOG_DBG(...)                                                           \
	do {                                                                       \
		if (DEBUG)                                                             \
			log_out(LL_DEBUG, true, __VA_ARGS__);                              \
	} while (0)
#define LOG_DBGL(lvl, ...)                                                     \
	do {                                                                       \
		if (DEBUG && conf.loglevel >= lvl)                                     \
			log_out(LL_DEBUG, true, __VA_ARGS__);                              \
	} while (0)
#define LOG_NL(lvl)                                                            \
	if (conf.loglevel == lvl)                                                  \
		log_out(lvl, false, "\n");

#endif

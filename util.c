/*
 * nrfdfu - Nordic DFU Upgrade Utility
 *
 * Copyright (C) 2020 Bruno Randolf (br1@einfach.org)
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "util.h"

#define DUMP_FMT "%x "
//#define DUMP_FMT "%d " /* dump data in same format as nrfutil (integer) */

void dump_data(const char* txt, const uint8_t* data, size_t len)
{
	printf("[ %s", txt);
	for (int i = 0; i < len; i++) {
		printf(DUMP_FMT, *(data + i));
	}
	printf("]\n");
}

static uint8_t hex_digit(char ch)
{
	if (('0' <= ch) && (ch <= '9')) {
		ch -= '0';
	} else {
		if (('a' <= ch) && (ch <= 'f')) {
			ch += 10 - 'a';
		} else {
			if (('A' <= ch) && (ch <= 'F')) {
				ch += 10 - 'A';
			} else {
				ch = 16;
			}
		}
	}
	return ch;
}

bool hex_to_bin(const char* hex, uint8_t* bin, size_t len)
{
	if (hex == NULL || bin == NULL) {
		return false;
	}

	for (size_t idx = 0; idx < len; ++idx) {
		bin[idx] = hex_digit(hex[2 * idx]) << 4;
		bin[idx] |= hex_digit(hex[1 + 2 * idx]);
	}
	return true;
}

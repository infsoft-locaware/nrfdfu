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

#ifndef DFUSER_H
#define DFUSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SLIP buffer size should be bigger than MTU */
#define BUF_SIZE	  100
#define SLIP_BUF_SIZE (BUF_SIZE * 2 + 1)

bool ser_enter_dfu(void);
bool ser_encode_write(uint8_t* req, size_t len);
const uint8_t* ser_read_decode(void);
void ser_fini(void);

#endif

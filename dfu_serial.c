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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "conf.h"
#include "dfu_serial.h"
#include "log.h"
#include "serialtty.h"
#include "slip.h"
#include "util.h"

#define MAX_READ_TRIES	   10000
#define SERIAL_TIMEOUT_SEC 1

static uint8_t buf[SLIP_BUF_SIZE];

extern int ser_fd;

bool ser_encode_write(uint8_t* req, size_t len)
{
	uint32_t slip_len;
	slip_encode(buf, (uint8_t*)req, len, &slip_len);

	serial_write(ser_fd, (const char*)buf, slip_len, SERIAL_TIMEOUT_SEC);

	if (conf.loglevel >= LL_DEBUG) {
		dump_data("TX: ", req, len);
	}

	return true;
}

const uint8_t* ser_read_decode(void)
{
	ssize_t ret;
	int end = 0;
	char read_buf;
	int read_tries = 0;
	bool timeout;

	slip_t slip = {.p_buffer = buf,
				   .current_index = 0,
				   .buffer_len = BUF_SIZE,
				   .state = SLIP_STATE_DECODING};

	do {
		read_tries++;
		timeout = serial_wait_read_ready(ser_fd, SERIAL_TIMEOUT_SEC);
		if (timeout) {
			LOG_INF("Timeout on Serial RX");
			break;
		}
		ret = read(ser_fd, &read_buf, 1);
		if (ret < 0 && errno != EAGAIN) {
			LOG_ERR("Read error: %d %s", errno, strerror(errno));
			break;
		} else if (ret > 0) {
			end = slip_decode_add_byte(&slip, read_buf);
		}
	} while (end != 1 || read_tries > MAX_READ_TRIES);

	if (conf.loglevel >= LL_DEBUG) {
		dump_data("RX: ", slip.p_buffer, slip.current_index);
	}

	return (end == 1 ? buf : NULL);
}

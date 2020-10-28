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
#include "dfu.h"
#include "dfu_serial.h"
#include "log.h"
#include "serialtty.h"
#include "slip.h"
#include "util.h"

#define DFU_SERIAL_BAUDRATE 115200
#define MAX_READ_TRIES		SLIP_BUF_SIZE
#define SERIAL_TIMEOUT_SEC	1

static uint8_t buf[SLIP_BUF_SIZE];
static int ser_fd = -1;
extern bool terminate;

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
	} while (end != 1 && read_tries < MAX_READ_TRIES && !terminate);

	if (conf.loglevel >= LL_DEBUG) {
		dump_data("RX: ", slip.p_buffer, slip.current_index);
	}

	return (end == 1 ? buf : NULL);
}

static bool serial_enter_dfu_cmd(void)
{
	char b[200];

	serial_set_baudrate(ser_fd, conf.serspeed);

	/* first read and discard anything that came before */
	read(ser_fd, b, 200);

	LOG_INF("Sending command to enter DFU mode: '%s'", conf.dfucmd);
	if (conf.dfucmd_hex) {
		hex_to_bin(conf.dfucmd, (uint8_t*)b, strlen(conf.dfucmd));
		size_t len = strlen(conf.dfucmd) / 2;
		serial_write(ser_fd, b, len, 1);
	} else {
		/* it looks like the first two characters written are lost...
		 * and we need \r to enter CLI */
		serial_write(ser_fd, "\r\r\r", 3, 1);
		serial_write(ser_fd, conf.dfucmd, strlen(conf.dfucmd), 1);
		serial_write(ser_fd, "\r", 1, 1);
	}
	sleep(1);

	int ret = read(ser_fd, b, 200);
	if (ret > 0) {
		if (!conf.dfucmd_hex) {
			/* debug output reply */
			b[ret--] = '\0';
			/* remove trailing \r \n */
			while (b[ret] == '\r' || b[ret] == '\n') {
				b[ret--] = '\0';
			}
			/* remove \r \n and zero from the beginning */
			ret = 0;
			while ((b[ret] == '\r' || b[ret] == '\n' || b[ret] == '\0')
				   && ret < sizeof(b)) {
				ret++;
			}
			LOG_INF("Device replied: '%s' (%d)", b + ret, ret);
		} else {
			LOG_INF("Device replied with %d bytes", ret);
		}

		serial_set_baudrate(ser_fd, DFU_SERIAL_BAUDRATE);
		return true;
	} else {
		LOG_INF("Device didn't repy (%d)", ret);
		serial_set_baudrate(ser_fd, DFU_SERIAL_BAUDRATE);
		return false;
	}
}

bool ser_enter_dfu(void)
{
	ser_fd = serial_init(conf.serport, DFU_SERIAL_BAUDRATE);
	if (ser_fd <= 0) {
		return false;
	}

	/* first check if Bootloader responds to Ping */
	LOG_NOTI_("Waiting for device to be ready: ");
	int ntry = 0;
	bool ret = false;
	do {
		if (conf.dfucmd) {
			ret = serial_enter_dfu_cmd();
			sleep(1);
			if (!ret) {
				/* if dfu command failed, try ping, it will
				 * usually fail with "Opcode not supported"
				 * because of the text we sent before, but then
				 * the next one below can succeed */
				ret = dfu_ping();
			}
		} else {
			sleep(1);
		}

		if (conf.loglevel < LL_INFO) {
			printf(".");
			fflush(stdout);
		}

		ret = dfu_ping();
	} while (!ret && ++ntry < conf.timeout && !terminate);

	LOG_NL(LL_NOTICE);

	if (ntry >= conf.timeout) {
		LOG_NOTI("Device didn't respond after %d tries", conf.timeout);
		return false;
	}
	return ret;
}

void ser_fini(void)
{
	serial_fini(ser_fd);
}

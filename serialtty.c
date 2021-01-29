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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"
#include "serialtty.h"

#define MAX_CONF_LEN 200

static struct termios tty;
static struct termios otty;

static void serial_set_tty_speed(int baud)
{
	// clang-format off
	switch (baud) {
		case 57600:		tty.c_cflag |= B57600; break;
		case 115200:	tty.c_cflag |= B115200; break;
		case 230400:	tty.c_cflag |= B230400; break;
		case 460800:	tty.c_cflag |= B460800; break;
		case 500000:	tty.c_cflag |= B500000; break;
		case 576000:	tty.c_cflag |= B576000; break;
		case 921600:	tty.c_cflag |= B921600; break;
		case 1000000:	tty.c_cflag |= B1000000; break;
		default:		LOG_ERR("Unknown baudrate %d", baud);
	}
	// clang-format on
}
int serial_init(const char* dev, int baud)
{
	int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		LOG_ERR("Couldn't open serial device '%s'", dev);
		return -1;
	}

	/* set necessary serial port attributes */
	memset(&tty, 0, sizeof tty);
	memset(&otty, 0, sizeof tty);

	if (tcgetattr(fd, &otty) != 0) {
		LOG_ERR("Couldn't get termio attrs");
		close(fd);
		return -1;
	}

	if (tcgetattr(fd, &tty) != 0) {
		LOG_ERR("Couldn't get termio attrs");
		close(fd);
		return -1;
	}

	tty.c_iflag = IGNPAR;
	tty.c_oflag = 0;
	tty.c_cflag = CLOCAL | CREAD | CS8;
	tty.c_lflag = 0;
	serial_set_tty_speed(baud);

	tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		LOG_ERR("Couldn't set termio attrs");
		close(fd);
		return -1;
	}

	return fd;
}

void serial_fini(int sock)
{
	if (sock < 0) {
		return;
	}

	/* unset DTR */
	int serialLines;
	ioctl(sock, TIOCMGET, &serialLines);
	serialLines &= ~TIOCM_DTR;
	ioctl(sock, TIOCMSET, &serialLines);

	/* reset terminal settings to original */
	if (tcsetattr(sock, TCSANOW, &otty) != 0) {
		LOG_ERR("Couldn't reset termio attrs");
	}

	close(sock);
}

bool serial_wait_read_ready(int fd, int sec)
{
	if (fd < 0) {
		return false;
	}

	struct timeval tv = {};
	fd_set fds = {};
	tv.tv_sec = sec;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	int ret = select(fd + 1, &fds, NULL, NULL, &tv);
	return ret <= 0; // error or timeout
}

bool serial_wait_write_ready(int fd, int sec)
{
	if (fd < 0) {
		return false;
	}

	struct timeval tv = {};
	fd_set fds = {};
	tv.tv_sec = sec;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	int ret = select(fd + 1, NULL, &fds, NULL, &tv);
	return ret <= 0; // error or timeout
}

/* write to serial handling blocking case */
bool serial_write(int fd, const char* buf, size_t len, int timeout_sec)
{
	ssize_t ret;
	size_t pos = 0;

	if (fd < 0) {
		return false;
	}

	do {
		ret = write(fd, buf + pos, len - pos);
		if (ret == -1) {
			if (errno == EAGAIN) {
				/* write would block, wait until ready again */
				serial_wait_write_ready(fd, timeout_sec);
				continue;
			} else {
				/* grave error */
				LOG_ERR("ERR: write error: %d %s", errno, strerror(errno));
				return false;
			}
		} else if ((size_t)ret < len - pos) {
			/* partial writes usually mean next write would return
			 * EAGAIN, so just wait until it's ready again */
			serial_wait_write_ready(fd, timeout_sec);
		}
		pos += ret;
	} while (pos < len);

	return true;
}

bool serial_set_baudrate(int fd, int baud)
{
	if (fd < 0) {
		return false;
	}

	tty.c_cflag = CLOCAL | CREAD | CS8;
	serial_set_tty_speed(baud);

	if (tcsetattr(fd, TCSAFLUSH, &tty) != 0) {
		LOG_ERR("Couldn't set termio attrs baudrate");
		return false;
	}
	return true;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <err.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include "serialtty.h"
#include "log.h"

#define MAX_CONF_LEN 200

static struct termios otty;

int serial_init(const char* dev)
{
	int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		LOG_ERR("Couldn't open serial device '%s'", dev);
		return -1;
	}

	/* set necessary serial port attributes */
	struct termios tty;
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
	tty.c_cflag = B115200 | CLOCAL | CREAD | CS8;
	tty.c_lflag = 0;

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
	if (sock < 0)
		return;

	/* unset DTR */
	int serialLines;
	ioctl(sock, TIOCMGET, &serialLines);
	serialLines &= ~TIOCM_DTR;
	ioctl(sock, TIOCMSET, & serialLines );

	/* reset terminal settings to original */
	if (tcsetattr(sock, TCSANOW, &otty) != 0)
		LOG_ERR("Couldn't reset termio attrs");

	close(sock);
}

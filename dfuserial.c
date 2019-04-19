#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>

#include "dfuserial.h"
#include "slip.h"
#include "log.h"
#include "conf.h"

#define MAX_READ_TRIES		10000
#define SERIAL_TIMEOUT_SEC	1

static uint8_t buf[SLIP_BUF_SIZE];

extern int ser_fd;

static bool wait_serial_write_ready(int sec)
{
	struct timeval tv = {};
	fd_set fds = {};
	tv.tv_sec = sec;
	FD_ZERO(&fds);
	FD_SET(ser_fd, &fds);
	int ret = select(ser_fd+1, NULL, &fds, NULL, &tv);
	return ret <= 0; // error or timeout
}

bool ser_encode_write(uint8_t* req, size_t len)
{
	uint32_t slip_len;
	ssize_t ret;
	size_t pos = 0;
	slip_encode(buf, (uint8_t*)req, len, &slip_len);
	do {
		ret = write(ser_fd, buf + pos, slip_len - pos);
		if (ret == -1) {
			if (errno == EAGAIN) {
				/* write would block, wait until ready again */
				wait_serial_write_ready(SERIAL_TIMEOUT_SEC);
				continue;
			} else {
				/* grave error */
				LOG_ERR("ERR: write error: %d %s", errno, strerror(errno));
				return false;
			}
		}
		else if (ret < slip_len - pos) {
			/* partial writes usually mean next write would return
			 * EAGAIN, so just wait until it's ready again */
			wait_serial_write_ready(1);
		}
		pos += ret;
	} while (pos < slip_len);

	if (conf.debug >= LL_DEBUG) {
		printf("[ TX: ");
		for (int i=0; i < len; i++) {
			printf("%d ", *(req+i));
		}
		printf("]\n");
	}

	return true;
}

static bool wait_serial_read_ready(int sec)
{
	struct timeval tv = {};
	fd_set fds = {};
	tv.tv_sec = sec;
	FD_ZERO(&fds);
	FD_SET(ser_fd, &fds);
	int ret = select(ser_fd+1, &fds, NULL, NULL, &tv);
	return ret <= 0; // error or timeout
}

const uint8_t* ser_read_decode(void)
{
	ssize_t ret;
	int end = 0;
	char read_buf;
	int read_tries = 0;
	bool timeout;

	slip_t slip = {
		.p_buffer = buf,
		.current_index = 0,
		.buffer_len = BUF_SIZE,
		.state = SLIP_STATE_DECODING
	};

	do {
		read_tries++;
		timeout = wait_serial_read_ready(SERIAL_TIMEOUT_SEC);
		if (timeout) {
			LOG_INF("Timeout on Serial RX");
			break;
		}
		ret = read(ser_fd, &read_buf, 1);
		if (ret < 0 && errno != EAGAIN) {
			LOG_ERR("Read error: %d %s", errno, strerror(errno));
			break;
		}
		else if (ret > 0) {
			end = slip_decode_add_byte(&slip, read_buf);
		}
	} while (end != 1 || read_tries > MAX_READ_TRIES);

	if (conf.debug >= LL_DEBUG) {
		printf("[ RX: ");
		for (int i=0; i < slip.current_index; i++) {
			printf("%d ", slip.p_buffer[i]);
		}
		printf("]\n");
	}

	return (end == 1 ? buf : NULL);
}

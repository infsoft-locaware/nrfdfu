#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>

#include "dfuserial.h"
#include "serialtty.h"
#include "slip.h"
#include "log.h"
#include "conf.h"

#define MAX_READ_TRIES		10000
#define SERIAL_TIMEOUT_SEC	1

static uint8_t buf[SLIP_BUF_SIZE];

extern int ser_fd;

bool ser_encode_write(uint8_t* req, size_t len)
{
	uint32_t slip_len;
	ssize_t ret;
	size_t pos = 0;
	slip_encode(buf, (uint8_t*)req, len, &slip_len);

	serial_write(ser_fd, buf, slip_len, SERIAL_TIMEOUT_SEC);

	if (conf.loglevel >= LL_DEBUG) {
		printf("[ TX: ");
		for (int i=0; i < len; i++) {
			printf("%d ", *(req+i));
		}
		printf("]\n");
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

	slip_t slip = {
		.p_buffer = buf,
		.current_index = 0,
		.buffer_len = BUF_SIZE,
		.state = SLIP_STATE_DECODING
	};

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
		}
		else if (ret > 0) {
			end = slip_decode_add_byte(&slip, read_buf);
		}
	} while (end != 1 || read_tries > MAX_READ_TRIES);

	if (conf.loglevel >= LL_DEBUG) {
		printf("[ RX: ");
		for (int i=0; i < slip.current_index; i++) {
			printf("%d ", slip.p_buffer[i]);
		}
		printf("]\n");
	}

	return (end == 1 ? buf : NULL);
}

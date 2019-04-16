#include <stdio.h>
#include <unistd.h>

#include "dfu.h"
#include "nrf_dfu_req_handler.h"
#include "log.h"
#include "slip.h"

#define BUF_SIZE	200
#define SLIP_BUF_SIZE	(BUF_SIZE * 2 + 1)
#define MAX_READ_TRIES	10000

static uint8_t write_buf[BUF_SIZE];
static uint8_t slip_buf[SLIP_BUF_SIZE];
extern int ser_fd;

static void encode_write(nrf_dfu_request_t* req, size_t len)
{
	uint32_t slip_len;
	ssize_t ret;
	slip_encode(slip_buf, (uint8_t*)req, len, &slip_len);
	ret = write(ser_fd, slip_buf, slip_len);
	if (ret < slip_len)
		LOG_ERR("write error");
}

static nrf_dfu_response_t* read_decode(void)
{
	ssize_t ret;
	slip_t slip;
	slip.p_buffer = write_buf;
	slip.current_index = 0;
	slip.buffer_len = BUF_SIZE;
	slip.state = SLIP_STATE_DECODING;
	int end = 0;
	int read_failed = 0;

	do {
		ret = read(ser_fd, slip_buf, 1);
		if (ret > 0) {
			end = slip_decode_add_byte(&slip, slip_buf[0]);
		} else {
			read_failed++;
		}
	} while (end != 1 && read_failed < MAX_READ_TRIES);

	if (end != 1) {
		return NULL;
	}

#if 1
	for (int i=0; i < slip.current_index; i++) {
		printf("0x%02x ", slip.p_buffer[i]);
	}
	printf("\n");
#endif
	
	if (slip.p_buffer[0] != NRF_DFU_OP_RESPONSE) {
		LOG_ERR("no response");
		return NULL;
	}
		
	return (nrf_dfu_response_t*)(slip.p_buffer + 1);
}

bool dfu_ping(void)
{
	static uint8_t ping_id = 1;
	LOG_INF("Sending ping %d", ping_id);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_PING,
		.ping.id = ping_id++,
	};
	encode_write(&req, 2);
	nrf_dfu_response_t* resp = read_decode();

	if (!resp) {
		LOG_ERR("No ping response");
		return false;
	}

	return (resp->request == NRF_DFU_OP_PING
		&& resp->result == NRF_DFU_RES_CODE_SUCCESS
		&& resp->ping.id == ping_id-1);
}

bool dfu_set_packet_receive_notification(uint32_t prn)
{
	LOG_INF("Set packet receive notification %d", prn);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_RECEIPT_NOTIF_SET,
		.prn.target = prn,
	};
	encode_write(&req, 3);
	nrf_dfu_response_t* resp = read_decode();

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	return (resp->request == NRF_DFU_OP_RECEIPT_NOTIF_SET
		&& resp->result == NRF_DFU_RES_CODE_SUCCESS);
}

bool dfu_get_serial_mtu(void)
{
	LOG_INF("Get serial MTU");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_MTU_GET,
	};
	encode_write(&req, 1);
	nrf_dfu_response_t* resp = read_decode();

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	if (resp->request == NRF_DFU_OP_MTU_GET
		&& resp->result == NRF_DFU_RES_CODE_SUCCESS) {
		LOG_INF("Serial MTU %d", resp->mtu.size);
	}
}

bool dfu_read_object(uint32_t type)
{
	LOG_INF("Read object %x", type);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_SELECT,
		.select.object_type = type,
	};
	encode_write(&req, 2);
	nrf_dfu_response_t* resp = read_decode();

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	if (resp->request == NRF_DFU_OP_OBJECT_SELECT
	    && resp->result == NRF_DFU_RES_CODE_SUCCESS) {
		LOG_INF("read obj offset: %d max_size: %d crc: %d",
			resp->select.offset, resp->select.max_size,
			resp->select.crc);
	}
}

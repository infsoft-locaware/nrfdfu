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
#if 1
	printf("[TX: ");
	for (int i=0; i < len; i++) {
		printf("0x%02x ", *(((uint8_t*)req)+i));
	}
	printf("]\n");
#endif

}

static bool read_decode(void)
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

#if 1
	printf("[RX: ");
	for (int i=0; i < slip.current_index; i++) {
		printf("0x%02x ", slip.p_buffer[i]);
	}
	printf("]\n");
#endif

	return (end == 1);
}

static nrf_dfu_response_t* get_response(nrf_dfu_op_t request)
{
	bool succ = read_decode();

	if (!succ) {
		return NULL;
	}

	if (write_buf[0] != NRF_DFU_OP_RESPONSE) {
		LOG_ERR("no response");
		return NULL;
	}

	nrf_dfu_response_t* resp = (nrf_dfu_response_t*)(write_buf + 1);

	if (resp->result != NRF_DFU_RES_CODE_SUCCESS) {
		return NULL;
	}

	if (resp->request != request) {
		LOG_ERR("Response does not match request");
		return NULL;
	}

	return resp;
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
	nrf_dfu_response_t* resp = get_response(req.request);

	if (!resp) {
		LOG_ERR("No ping response");
		return false;
	}

	return (resp->ping.id == ping_id-1);
}

bool dfu_set_packet_receive_notification(uint32_t prn)
{
	LOG_INF("Set packet receive notification %d", prn);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_RECEIPT_NOTIF_SET,
		.prn.target = prn,
	};
	encode_write(&req, 3);
	nrf_dfu_response_t* resp = get_response(req.request);

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	return true;
}

bool dfu_get_serial_mtu(void)
{
	LOG_INF("Get serial MTU");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_MTU_GET,
	};
	encode_write(&req, 1);
	nrf_dfu_response_t* resp = get_response(req.request);

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	LOG_INF("Serial MTU %d", resp->mtu.size);
}

bool dfu_read_object(uint32_t type)
{
	LOG_INF("Read object %x", type);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_SELECT,
		.select.object_type = type,
	};
	encode_write(&req, 2);
	nrf_dfu_response_t* resp = get_response(req.request);

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	LOG_INF("read obj offset: %d max_size: %d crc: %d",
		resp->select.offset, resp->select.max_size, resp->select.crc);
}

bool dfu_create_object(uint32_t type, uint32_t size)
{
	LOG_INF("Create object %x (%d)", type, size);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_CREATE,
		.create.object_type = type,
		.create.object_size = size,
	};
	LOG_INF("sixz %zd", sizeof(req.create));
	encode_write(&req, 1 + sizeof(req.create));
	nrf_dfu_response_t* resp = get_response(req.request);

	if (!resp) {
		LOG_ERR("No response");
		return false;
	}

	LOG_INF("create obj");
	//TODO: Definition in nrf_dfu_req_handler.h is wrong!
	//LOG_INF("create obj offset: %d crc: %d",
	//	resp->create.offset, resp->create.crc);
}

#include <stdio.h>
#include <unistd.h>

#include "dfu.h"
#include "nrf_dfu_req_handler.h"
#include "log.h"
#include "slip.h"
#include "conf.h"
#include "util.h"

/* SLIP buffer size should be bigger than MTU */
#define BUF_SIZE	100
#define SLIP_BUF_SIZE	(BUF_SIZE * 2 + 1)
#define MAX_READ_TRIES	10000

static uint8_t buf[SLIP_BUF_SIZE];
static uint16_t mtu;
static uint32_t max_size;
extern int ser_fd;

static bool encode_write(uint8_t* req, size_t len)
{
	uint32_t slip_len;
	ssize_t ret;
	slip_encode(buf, (uint8_t*)req, len, &slip_len);
	ret = write(ser_fd, buf, slip_len);
	if (ret < slip_len) {
		LOG_ERR("write error");
		return false;
	}

	if (conf.debug > 2) {
		printf("[ TX: ");
		for (int i=0; i < len; i++) {
			printf("%d ", *(req+i));
		}
		printf("]\n");
	}

	return true;
}

static size_t request_size(nrf_dfu_request_t* req)
{
	switch (req->request) {
		case NRF_DFU_OP_OBJECT_CREATE:
			return 1 + sizeof(req->create);
		case NRF_DFU_OP_RECEIPT_NOTIF_SET:
			return 1 + sizeof(req->prn);
		case NRF_DFU_OP_OBJECT_SELECT:
			return 1 + sizeof(req->select);
		case NRF_DFU_OP_MTU_GET:
			return 1; // NOT sizeof(req->mtu);
		case NRF_DFU_OP_OBJECT_WRITE:
			return 1 + sizeof(req->write);
		case NRF_DFU_OP_PING:
			return 1 + sizeof(req->ping);
		case NRF_DFU_OP_FIRMWARE_VERSION:
			return 1 + sizeof(req->firmware);
		case NRF_DFU_OP_PROTOCOL_VERSION:
		case NRF_DFU_OP_CRC_GET:
		case NRF_DFU_OP_OBJECT_EXECUTE:
		case NRF_DFU_OP_HARDWARE_VERSION:
		case NRF_DFU_OP_ABORT:
		case NRF_DFU_OP_RESPONSE:
		case NRF_DFU_OP_INVALID:
			return 1;
	}
	return 0;
}

static bool send_request(nrf_dfu_request_t* req)
{
	size_t size = request_size(req);
	if (size == 0) {
		LOG_ERR("Unknown size");
		return false;
	}

	return encode_write((uint8_t*)req, size);
}

static bool read_decode(void)
{
	ssize_t ret;
	slip_t slip;
	slip.p_buffer = buf;
	slip.current_index = 0;
	slip.buffer_len = BUF_SIZE;
	slip.state = SLIP_STATE_DECODING;
	int end = 0;
	int read_failed = 0;
	char read_buf;

	do {
		ret = read(ser_fd, &read_buf, 1);
		if (ret > 0) {
			end = slip_decode_add_byte(&slip, read_buf);
			if (end != -3) { // if not "busy": error or finished
				break;
			}
		} else {
			read_failed++;
		}
	} while (end != 1 && read_failed < MAX_READ_TRIES);

	if (conf.debug > 2) {
		printf("[ RX: ");
		for (int i=0; i < slip.current_index; i++) {
			printf("%d ", slip.p_buffer[i]);
		}
		printf("]\n");
	}

	return (end == 1);
}

static const char* dfu_err_str(nrf_dfu_result_t res)
{
	switch (res) {
		case NRF_DFU_RES_CODE_INVALID:
			return "Invalid opcode";
		case NRF_DFU_RES_CODE_SUCCESS:
			return "Operation successful";
		case NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED:
			return "Opcode not supported";
		case NRF_DFU_RES_CODE_INVALID_PARAMETER:
			return "Missing or invalid parameter value";
		case NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES:
			return "Not enough memory for the data object";
		case NRF_DFU_RES_CODE_INVALID_OBJECT:
			return "Data object does not match the firmware and "
				"hardware requirements, the signature is wrong, "
				"or parsing the command failed";
		case NRF_DFU_RES_CODE_UNSUPPORTED_TYPE:
			return "Not a valid object type for a Create request";
		case NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED:
			return "The state of the DFU process does not allow this "
				"operation";
		case NRF_DFU_RES_CODE_OPERATION_FAILED:
			return "Operation failed";
		case NRF_DFU_RES_CODE_EXT_ERROR:
			return "Extended error";
			/* The next byte of the response contains the error code
			 * of the extended error (see @ref nrf_dfu_ext_error_code_t. */
	}
	return "Unknown error";
}

static nrf_dfu_response_t* get_response(nrf_dfu_op_t request)
{
	bool succ = read_decode();
	if (!succ) {
		LOG_ERR("Read or decode failed");
		return NULL;
	}

	if (buf[0] != NRF_DFU_OP_RESPONSE) {
		LOG_ERR("No response");
		return NULL;
	}

	nrf_dfu_response_t* resp = (nrf_dfu_response_t*)(buf + 1);

	if (resp->result != NRF_DFU_RES_CODE_SUCCESS) {
		LOG_ERR("Response Error %s", dfu_err_str(resp->result));
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

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	return (resp->ping.id == ping_id-1);
}

bool dfu_set_packet_receive_notification(uint16_t prn)
{
	LOG_INF("Set packet receive notification %d", prn);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_RECEIPT_NOTIF_SET,
		.prn.target = prn,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
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

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	mtu = resp->mtu.size;
	if (mtu > SLIP_BUF_SIZE) {
		LOG_WARN("MTU of %d limited to buffer size %d", mtu, SLIP_BUF_SIZE);
		mtu = SLIP_BUF_SIZE;
	}
	LOG_INF("Serial MTU %d", mtu);
	return true;
}

bool dfu_select_object(uint8_t type)
{
	LOG_INF("Select object %x", type);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_SELECT,
		.select.object_type = type,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	LOG_INF("Select object offset: %d max_size: %d crc: %d",
		resp->select.offset, resp->select.max_size, resp->select.crc);
	max_size = resp->select.max_size;
	return true;
}

bool dfu_create_object(uint8_t type, uint32_t size)
{
	LOG_INF("Create object %x (%d)", type, size);
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_CREATE,
		.create.object_type = type,
		.create.object_size = size,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	LOG_INF("create obj");
	//TODO: Definition in nrf_dfu_req_handler.h is wrong!
	//LOG_INF("create obj offset: %d crc: %d",
	//	resp->create.offset, resp->create.crc);
	return true;
}


bool dfu_object_write(FILE* fp)
{
	uint8_t fbuf[(mtu-1)/2];
	size_t written = 0;

	LOG_INF("Write data (MTU %d buf %zd)", mtu, sizeof(fbuf));

	do {
		fbuf[0] = NRF_DFU_OP_OBJECT_WRITE;
		size_t len = fread(fbuf + 1, 1, sizeof(fbuf) - 1, fp);
		if (len == 0) {
			LOG_ERR("fread error");
			break;
		}
		bool b = encode_write(fbuf, len + 1);
		if (!b) {
			LOG_ERR("enc failed");
			return false;
		}
		written += len;
		LOG_INF("wrote %zd", len);
	} while (!feof(fp) && written < max_size);

	// No response expected
	LOG_INF("Wrote %zd bytes", written);
	return true;
}

bool dfu_get_crc(void)
{
	LOG_INF("Get CRC");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_CRC_GET,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	sleep(1); // TODO

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	LOG_INF("Got CRC %d offset %d", resp->crc.crc, resp->crc.offset);
	return true;
}

/** this writes the object to flash */
bool dfu_object_execute(void)
{
	LOG_INF("Object Execute");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_EXECUTE,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	sleep(1); // TODO

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	return true;
}

static size_t get_file_size(FILE* fp)
{
	fseek(fp, 0L, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	LOG_INF("Size %zd", sz);
	return sz;
}

bool dfu_object_write_procedure(uint8_t type, FILE* fp)
{
	size_t sz = get_file_size(fp);

	dfu_select_object(type);

	/* create and write objects of max_size */
	for (int i=0; i < sz; i += max_size) {
		dfu_create_object(type, MIN(sz-i, max_size));
		dfu_object_write(fp);
		dfu_get_crc();
		dfu_object_execute();
	}
}

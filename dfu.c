#include <zlib.h>

#include "dfu.h"
#include "dfuserial.h"
#include "nrf_dfu_req_handler.h"
#include "log.h"
#include "util.h"

static uint16_t dfu_mtu;
static uint32_t dfu_max_size;
static uLong dfu_current_crc;

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

	return ser_encode_write((uint8_t*)req, size);
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
	const uint8_t* buf = ser_read_decode();
	if (!buf) {
		/* error printed in function above */
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
	LOG_INF_("Sending ping %d: ", ping_id);
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

	if (resp->ping.id == ping_id-1) {
		LOG_INF("OK");
	} else {
		LOG_INF("Failed");
	}
	return (resp->ping.id == ping_id-1);
}

bool dfu_set_packet_receive_notification(uint16_t prn)
{
	LOG_INF_("Set packet receive notification %d: ", prn);
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

	LOG_INF("OK");
	return true;
}

bool dfu_get_serial_mtu(void)
{
	LOG_INF_("Get serial MTU: ");
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

	dfu_mtu = resp->mtu.size;
	if (dfu_mtu > SLIP_BUF_SIZE) {
		LOG_WARN("MTU of %d limited to buffer size %d", dfu_mtu,
			 SLIP_BUF_SIZE);
		dfu_mtu = SLIP_BUF_SIZE;
	}
	LOG_INF("%d", dfu_mtu);
	return true;
}

bool dfu_select_object(uint8_t type)
{
	LOG_INF_("Select object %d: ", type);
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

	LOG_INF("offset %u max_size %u CRC 0x%X",
		resp->select.offset, resp->select.max_size, resp->select.crc);
	dfu_max_size = resp->select.max_size;
	return true;
}

bool dfu_create_object(uint8_t type, uint32_t size)
{
	LOG_INF_("Create object %d (size %u): ", type, size);
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

	LOG_INF("OK");
	return true;
}


bool dfu_object_write(zip_file_t* zf, size_t size)
{
	uint8_t fbuf[(dfu_mtu-1)/2];
	size_t written = 0;
	zip_int64_t len;

	LOG_INF_("Write data (MTU %d buf %zd): ", dfu_mtu, sizeof(fbuf));

	do {
		fbuf[0] = NRF_DFU_OP_OBJECT_WRITE;
		len = zip_fread(zf, fbuf + 1, sizeof(fbuf) - 1);
		if (len < 0) {
			LOG_ERR("zip_fread error");
			break;
		}
		if (len == 0) { // EOF
			break;
		}
		bool b = ser_encode_write(fbuf, len + 1);
		if (!b) {
			LOG_ERR("write failed");
			return false;
		}
		written += len;
		dfu_current_crc = crc32(dfu_current_crc, fbuf+1, len);
	} while (len > 0 && written < size && written < dfu_max_size);

	// No response expected
	LOG_INF("%zd bytes CRC: 0x%lX", written, dfu_current_crc);
	return true;
}

uint32_t dfu_get_crc(void)
{
	LOG_INF_("Get CRC: ");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_CRC_GET,
	};

	bool b = send_request(&req);
	if (!b) {
		return 0;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return 0;
	}

	LOG_INF("0x%X (offset %u)", resp->crc.crc, resp->crc.offset);
	return resp->crc.crc;
}

/** this writes the object to flash */
bool dfu_object_execute(void)
{
	LOG_INF_("Object Execute: ");
	nrf_dfu_request_t req = {
		.request = NRF_DFU_OP_OBJECT_EXECUTE,
	};

	bool b = send_request(&req);
	if (!b) {
		return false;
	}

	nrf_dfu_response_t* resp = get_response(req.request);
	if (!resp) {
		return false;
	}

	LOG_INF("OK");
	return true;
}

bool dfu_object_write_procedure(uint8_t type, zip_file_t* zf, size_t sz)
{
	dfu_select_object(type);
	dfu_current_crc = crc32(0L, Z_NULL, 0);

	/* create and write objects of max_size */
	for (int i=0; i < sz; i += dfu_max_size) {
		dfu_create_object(type, MIN(sz-i, dfu_max_size));
		dfu_object_write(zf, sz);
		uint32_t rcrc = dfu_get_crc();
		if (rcrc != dfu_current_crc) {
			LOG_ERR("CRC failed 0x%X vs 0x%lX", rcrc, dfu_current_crc);
			return false;
		}
		dfu_object_execute();
	}
}

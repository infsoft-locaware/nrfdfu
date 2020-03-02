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

#include <endian.h>

#include <zlib.h>

#include "conf.h"
#include "dfu.h"
#include "dfuserial.h"
#include "log.h"
#include "nrf_dfu_handling_error.h"
#include "nrf_dfu_req_handler.h"
#include "util.h"

static uint16_t dfu_mtu;
static uint32_t dfu_max_size;
static uint32_t dfu_current_crc;

static size_t request_size(nrf_dfu_request_t *req)
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

static bool send_request(nrf_dfu_request_t *req)
{
    size_t size = request_size(req);
    if (size == 0) {
        LOG_ERR("Unknown size");
        return false;
    }

    return ser_encode_write((uint8_t *)req, size);
}

static const char *dfu_err_str(nrf_dfu_result_t res)
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

static const char *dfu_ext_err_str(nrf_dfu_ext_error_code_t res)
{
    switch (res) {
    case NRF_DFU_EXT_ERROR_NO_ERROR:
        return "No extended error code has been set.";
    case NRF_DFU_EXT_ERROR_INVALID_ERROR_CODE:
        return "Invalid error code.";
    case NRF_DFU_EXT_ERROR_WRONG_COMMAND_FORMAT:
        return "The format of the command was incorrect.";
    case NRF_DFU_EXT_ERROR_UNKNOWN_COMMAND:
        return "The command was successfully parsed, but it is "
               "not supported or unknown";
    case NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID:
        return "The init command is invalid. The init packet "
               "either has an invalid update type or it is "
               "missing required fields for the update type "
               "for example, the init packet for a SoftDevice "
               "update is missing the SoftDevice size field.";
    case NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE:
        return "The firmware version is too low. For an "
               "application or SoftDevice, the version must be "
               "greater than or equal to the current version. "
               "For a bootloader, it must be greater than the "
               "current version. This requirement prevents "
               " downgrade attacks.";
    case NRF_DFU_EXT_ERROR_HW_VERSION_FAILURE:
        return "The hardware version of the device does not "
               "match the required hardware version for the "
               "update.";
    case NRF_DFU_EXT_ERROR_SD_VERSION_FAILURE:
        return "The array of supported SoftDevices for the "
               "update does not contain the FWID of the "
               "current SoftDevice or the first FWID is '0' on "
               "a bootloader which requires the SoftDevice to "
               "be present.";
    case NRF_DFU_EXT_ERROR_SIGNATURE_MISSING:
        return "The init packet does not contain a signature.";
    case NRF_DFU_EXT_ERROR_WRONG_HASH_TYPE:
        return "The hash type that is specified by the init "
               "packet is not supported by the DFU bootloader.";
    case NRF_DFU_EXT_ERROR_HASH_FAILED:
        return "The hash of the firmware image cannot be "
               "calculated.";
    case NRF_DFU_EXT_ERROR_WRONG_SIGNATURE_TYPE:
        return "The type of the signature is unknown or not "
               "supported by the DFU bootloader.";
    case NRF_DFU_EXT_ERROR_VERIFICATION_FAILED:
        return "The hash of the received firmware image does "
               "not match the hash in the init packet.";
    case NRF_DFU_EXT_ERROR_INSUFFICIENT_SPACE:
        return "The available space on the device is "
               "insufficient to hold the firmware.";
    }
    return "Unknown extended error";
}

static nrf_dfu_response_t *get_response(nrf_dfu_op_t request)
{
    const uint8_t *buf = ser_read_decode();
    if (!buf) {
        /* error printed in function above */
        return NULL;
    }

    if (buf[0] != NRF_DFU_OP_RESPONSE) {
        LOG_ERR("No response");
        return NULL;
    }

    nrf_dfu_response_t *resp = (nrf_dfu_response_t *)(buf + 1);

    if (resp->result != NRF_DFU_RES_CODE_SUCCESS) {
        LOG_ERR("Response Error %s", dfu_err_str(resp->result));
        if (resp->result == NRF_DFU_RES_CODE_EXT_ERROR) {
            LOG_ERR("ERR: '%s'", dfu_ext_err_str(resp->ext_err));
        }
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

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return false;
    }

    if (resp->ping.id == ping_id - 1) {
        LOG_INF("OK");
    } else {
        LOG_INF("Failed");
    }
    return (resp->ping.id == ping_id - 1);
}

bool dfu_set_packet_receive_notification(uint16_t prn)
{
    LOG_INF_("Set packet receive notification %d: ", prn);
    nrf_dfu_request_t req = {
        .request = NRF_DFU_OP_RECEIPT_NOTIF_SET,
        .prn.target = htole16(prn),
    };

    bool b = send_request(&req);
    if (!b) {
        return false;
    }

    nrf_dfu_response_t *resp = get_response(req.request);
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

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return false;
    }

    dfu_mtu = le16toh(resp->mtu.size);
    if (dfu_mtu > SLIP_BUF_SIZE) {
        LOG_WARN("MTU of %d limited to buffer size %d", dfu_mtu,
                 SLIP_BUF_SIZE);
        dfu_mtu = SLIP_BUF_SIZE;
    }
    LOG_INF("%d", dfu_mtu);
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

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return 0;
    }

    LOG_INF("0x%X (offset %u)", le32toh(resp->crc.crc), le32toh(resp->crc.offset));
    return le32toh(resp->crc.crc);
}

bool dfu_object_select(uint8_t type, uint32_t *offset, uint32_t *crc)
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

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return false;
    }

    dfu_max_size = le32toh(resp->select.max_size);
    *offset = le32toh(resp->select.offset);
    *crc = le32toh(resp->select.crc);
    LOG_INF("offset %u max_size %u CRC 0x%X", *offset, dfu_max_size, *crc);
    return true;
}

bool dfu_object_create(uint8_t type, uint32_t size)
{
    LOG_INF_("Create object %d (size %u): ", type, size);
    nrf_dfu_request_t req = {
        .request = NRF_DFU_OP_OBJECT_CREATE,
        .create.object_type = type,
        .create.object_size = htole32(size),
    };

    bool b = send_request(&req);
    if (!b) {
        return false;
    }

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return false;
    }

    LOG_INF("OK");
    return true;
}

bool dfu_object_write(zip_file_t *zf, size_t size)
{
    uint8_t fbuf[(dfu_mtu - 1) / 2];
    size_t written = 0;
    zip_int64_t len;
    size_t to_read;

    LOG_INF_("Write data (MTU %d buf %zd): ", dfu_mtu, sizeof(fbuf));

    do {
        fbuf[0] = NRF_DFU_OP_OBJECT_WRITE;
        to_read = MIN(sizeof(fbuf) - 1, size - written);
        len = zip_fread(zf, fbuf + 1, to_read);
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
        dfu_current_crc = crc32(dfu_current_crc, fbuf + 1, len);
    } while (len > 0 && written < size && written < dfu_max_size);

    // No response expected
    LOG_INF("%zd bytes CRC: 0x%X", written, dfu_current_crc);

    if (conf.loglevel < LL_INFO) {
        printf(".");
        fflush(stdout);
    }

    return true;
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

    nrf_dfu_response_t *resp = get_response(req.request);
    if (!resp) {
        return false;
    }

    LOG_INF("OK");
    return true;
}

/* get CRC of contents of ZIP file until size */
static uint32_t zip_crc_move(zip_file_t *zf, size_t size)
{
    uint8_t fbuf[200];
    size_t read = 0;
    size_t to_read;
    zip_int64_t len;
    uint32_t crc = crc32(0L, Z_NULL, 0);

    do {
        to_read = MIN(sizeof(fbuf), size - read);
        len = zip_fread(zf, fbuf, to_read);
        if (len < 0) {
            LOG_ERR("zip_fread error");
            break;
        }
        if (len == 0) { // EOF
            break;
        }
        read += len;
        crc = crc32(crc, fbuf, len);
    } while (len > 0 && read < size);

    return crc;
}

bool dfu_object_write_procedure(uint8_t type, zip_file_t *zf, size_t sz)
{
    uint32_t offset;
    uint32_t crc;

    if (!dfu_object_select(type, &offset, &crc))
        return false;

    /* object with same length and CRC already received */
    if (offset == sz && zip_crc_move(zf, sz) == crc) {
        LOG_NOTI_("Object already received");
        /* Don't transfer anything and skip to the Execute command */
        return dfu_object_execute();
    }

    /* parts already received */
    if (offset > 0) {
        uint32_t remain = offset % dfu_max_size;
        LOG_WARN("Object partially received (offset %u remaining %u)",
                 offset, remain);

        dfu_current_crc = zip_crc_move(zf, offset);
        if (crc != dfu_current_crc) {
            /* invalid crc, remove corrupted data, rewind and
			 * create new object below */
            offset -= remain > 0 ? remain : dfu_max_size;
            LOG_WARN("CRC does not match (restarting from %u)", offset);
            zip_fseek(zf, 0, 0);
            dfu_current_crc = zip_crc_move(zf, offset);
        } else if (offset < sz) { /* CRC matches */
            /* transfer remaining data if necessary */
            if (remain > 0) {
                size_t end = offset + dfu_max_size - remain;
                if (!dfu_object_write(zf, end))
                    return false;
            }
            if (!dfu_object_execute())
                return false;
        }
    } else if (offset == 0) {
        dfu_current_crc = crc32(0L, Z_NULL, 0);
    }

    /* create and write objects of max_size */
    for (int i = offset; i < sz; i += dfu_max_size) {
        if (!dfu_object_create(type, MIN(sz - i, dfu_max_size)))
            return false;

        if (!dfu_object_write(zf, sz))
            return false;

        uint32_t rcrc = dfu_get_crc();
        if (rcrc != dfu_current_crc) {
            LOG_ERR("CRC failed 0x%X vs 0x%X", rcrc, dfu_current_crc);
            return false;
        }

        if (!dfu_object_execute())
            return false;
    }

    return true;
}

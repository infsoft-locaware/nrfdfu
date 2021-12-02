#include <string.h>
#include <unistd.h>

#include "blzlib.h"
#include <blzlib_util.h>

#include "conf.h"
#include "dfu_legacy.h"
#include "log.h"
#include "util.h"

// Legacy DFU
#define DFU_SERVICE_UUID "00001530-1212-EFDE-1523-785FEABCD123"
#define DFU_CONTROL_UUID "00001531-1212-EFDE-1523-785FEABCD123"
#define DFU_DATA_UUID	 "00001532-1212-EFDE-1523-785FEABCD123"
//#define DFU_BUTTONLESS_UUID "00001533-1212-EFDE-1523-785FEABCD123"

#define CONNECT_NORMAL_TRY	3
#define CONNECT_DFUTARG_TRY 10

static bool terminate = false;
static bool control_noti = false;
static bool disconnect_noti = false;
static blz_ctx* ctx = NULL;
static blz_dev* dev = NULL;
static blz_serv* srv = NULL;
static blz_char* cp = NULL;
static blz_char* dp = NULL;

static uint8_t recv_buf[200];

void lcontrol_notify_handler(const uint8_t* data, size_t len, blz_char* ch,
							 void* user)
{
	memcpy(recv_buf, data, len);
	control_noti = true;

	if (conf.loglevel >= LL_DEBUG) {
		dump_data("RX: ", data, len);
	}
}

static void ldisconnect_handler(void* user)
{
	// LOG_NOTI("*disconnected*");
	disconnect_noti = true;
}

static blz_dev* lretry_connect(const char* address, enum BLE_ATYPE atype,
							   int tries)
{
	blz_dev* dev = NULL;
	int trynum = 0;

	do {
		if (trynum > 0) {
			LOG_ERR("Retry connecting to %s", address);
			sleep(5);
		}
		dev = blz_connect(ctx, address, atype);
	} while (dev == NULL && ++trynum < tries && !terminate);

	if (trynum >= tries) {
		LOG_ERR("Gave up connecting to %s after %d tries", address, trynum);
	}

	return dev;
}

bool lble_write_ctrl(uint8_t* req, size_t len)
{
	if (conf.loglevel >= LL_DEBUG) {
		dump_data("CP: ", req, len);
	}
	return blz_char_write(cp, req, len);
}

bool lble_write_data(uint8_t* req, size_t len)
{
	if (conf.loglevel >= LL_DEBUG) {
		dump_data("TX: ", req, len);
	}
	return blz_char_write_cmd(dp, req, len);
}

const uint8_t* lble_read(void)
{
	/* wait until notification is received */
	control_noti = false;
	blz_loop_wait(ctx, &control_noti, 10000);
	if (!control_noti) {
		LOG_ERR("BLE waiting for notification failed");
		return NULL;
	}

	return recv_buf;
}

enum dfu_ret ldfu_start(zip_file_t* sd_zip, size_t sd_size, zip_file_t* app_zip,
						size_t app_size)
{
	LOG_NOTI("DFU LEGACY");
	uint8_t buf[20];

	ctx = blz_init(conf.interface);
	if (ctx == NULL) {
		LOG_ERR("Could not initialize BLE interface '%s'", conf.interface);
		return false;
	}

	LOG_NOTI("Connecting to %s (%s)...", conf.ble_addr,
			 blz_addr_type_str(conf.ble_atype));
	dev = lretry_connect(conf.ble_addr, conf.ble_atype, CONNECT_NORMAL_TRY);
	if (dev == NULL) {
		return false;
	}

	blz_set_disconnect_handler(dev, ldisconnect_handler, NULL);

	srv = blz_get_serv_from_uuid(dev, DFU_SERVICE_UUID);
	if (srv == NULL) {
		LOG_ERR("DFU Service not found");
		return false;
	}

	cp = blz_get_char_from_uuid(srv, DFU_CONTROL_UUID);
	if (cp == NULL) {
		LOG_ERR("Could not find DFU CP UUID");
	}

	dp = blz_get_char_from_uuid(srv, DFU_DATA_UUID);
	if (dp == NULL) {
		LOG_ERR("Could not find DFU DP UUID");
	}

	bool b = blz_char_notify_start(cp, lcontrol_notify_handler, NULL);
	if (!b) {
		LOG_ERR("Could not start notification");
		return false;
	}

	LOG_NOTI("Start DFU");
	/* Start DFU 0x01:
	 * 0x00	No Image	No image will be updated.
	 * 0x01	SoftDevice	A SoftDevice image will be transferred.
	 * 0x02	Bootloader	A bootloader image will be transferred.
	 * 0x03	SoftDevice Bootloader	A SoftDevice with Bootloader image will be
	 * transferred. 0x04	Application	An application image will be
	 * transferred.
	 */
	buf[0] = 0x01;
	buf[1] = 0x03;
	b = blz_char_write(cp, buf, 2);
	if (!b) {
		LOG_ERR("Could not write start");
		return false;
	}

	*((uint32_t*)&buf[0]) = sd_size; // softdevice
	*(uint32_t*)&buf[4] = 1000;	// bootloader
	*(uint32_t*)&buf[8] = 0;//app_size;	// application

	hex_dump("BUF ", buf, 12);
	lble_write_data(buf, 12);
	lble_read();

	blz_disconnect(dev);
	blz_fini(ctx);
	return true;
}

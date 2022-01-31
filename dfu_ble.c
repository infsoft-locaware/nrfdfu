/*
 * nrfdfu - Nordic DFU Upgrade Utility
 *
 * Copyright (C) 2020 Bruno Randolf (br1@einfach.org)
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "dfu_ble.h"
#include "log.h"
#include "util.h"

#ifndef BLE_SUPPORT

int ble_enter_dfu(const char* interface, const char* address,
				  enum BLE_ATYPE atype)
{
	return false;
}
bool ble_connect_dfu_targ(const char* interface, const char* address,
						  enum BLE_ATYPE atype)
{
	return false;
}
bool ble_write_ctrl(uint8_t* req, size_t len)
{
	return false;
}
bool ble_write_data(uint8_t* req, size_t len)
{
	return false;
}
const uint8_t* ble_read(void)
{
	return NULL;
}
void ble_disconnect(void)
{
}
void ble_fini(void)
{
}

#else

#include <blzlib.h>
#include <blzlib_util.h>

#define DFU_SERVICE_UUID	 "0000fe59-0000-1000-8000-00805f9b34fb"
#define DFU_CONTROL_UUID	 "8EC90001-F315-4F60-9FB8-838830DAEA50"
#define DFU_DATA_UUID		 "8EC90002-F315-4F60-9FB8-838830DAEA50"
#define DFU_BUTTONLESS_UUID	 "8EC90003-F315-4F60-9FB8-838830DAEA50"
#define SERVICE_CHANGED_UUID "2A05"
#define CONNECT_NORMAL_TRY	 3
#define CONNECT_DFUTARG_TRY	 10

static bool terminate = false;
static bool buttonless_noti = false;
static bool control_noti = false;
static bool disconnect_noti = false;
static blz_ctx* ctx = NULL;
static blz_dev* dev = NULL;
static blz_serv* srv = NULL;
static blz_char* cp = NULL;
static blz_char* dp = NULL;

static uint8_t recv_buf[200];

void buttonless_notify_handler(const uint8_t* data, size_t len, blz_char* ch,
							   void* user)
{
	if (data[2] != 0x01) {
		LOG_ERR("Unexpected response (%zd) %x %x %x", len, data[0], data[1],
				data[2]);
	}
	buttonless_noti = true;
}

void control_notify_handler(const uint8_t* data, size_t len, blz_char* ch,
							void* user)
{
	memcpy(recv_buf, data, len);
	control_noti = true;

	if (conf.loglevel >= LL_DEBUG) {
		dump_data("RX: ", data, len);
	}
}

static void connect_handler(bool conn, void* user)
{
	if (!conn) {
		// LOG_NOTI("*disconnected*");
		disconnect_noti = true;
	}
}

static blz_dev* retry_connect(const char* address, enum BLE_ATYPE atype,
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

static bool start_cp_notify()
{
	bool b = blz_char_notify_start(cp, control_notify_handler, NULL);
	if (!b) {
		LOG_ERR("Could not start CP notification");
		return false;
	}
	return true;
}

/** returns 0 on error, 1 on success and 2 when already in bootloader */
int ble_enter_dfu(const char* interface, const char* address,
				  enum BLE_ATYPE atype)
{
	ctx = blz_init(interface);
	if (ctx == NULL) {
		LOG_ERR("Could not initialize BLE interface '%s'", interface);
		return false;
	}

	LOG_NOTI("Connecting to %s (%s)...", address, blz_addr_type_str(atype));
	dev = retry_connect(address, atype, CONNECT_NORMAL_TRY);
	if (dev == NULL) {
		return false;
	}

	blz_set_connect_handler(dev, connect_handler, NULL);

	srv = blz_get_serv_from_uuid(dev, DFU_SERVICE_UUID);
	if (srv == NULL) {
		LOG_ERR("DFU Service not found");
		return false;
	}

	blz_char* bch = blz_get_char_from_uuid(srv, DFU_BUTTONLESS_UUID);
	if (bch == NULL) {
		LOG_ERR("Could not find buttonless DFU UUID");
		/* try to find characteristics of DfuTarg */
		dp = blz_get_char_from_uuid(srv, DFU_DATA_UUID);
		cp = blz_get_char_from_uuid(srv, DFU_CONTROL_UUID);
		if (dp != NULL && cp != NULL) {
			LOG_NOTI("Device already is in Bootloader");
			if (!start_cp_notify()) {
				return false;
			} else {
				return 2; /* already in bootloader */
			}
		} else {
			return false;
		}
	}

	bool b = blz_char_indicate_start(bch, buttonless_notify_handler, NULL);
	if (!b) {
		LOG_ERR("Could not start buttonless notification");
		return false;
	}

	LOG_NOTI("Enter DFU Bootloader");

	uint8_t buf = 0x01;
	b = blz_char_write(bch, &buf, 1);
	if (!b) {
		LOG_ERR("Could not write buttonless");
		return false;
	}

	/* wait until notification is received with confirmation */
	blz_loop_wait(ctx, &buttonless_noti, 10000);
	if (!buttonless_noti) {
		LOG_ERR("Timed out waiting for confirmation");
		return false;
	}

	/* we don't disconnect here, because the device will reset and enter
	 * bootloader and appear under a new MAC and the connection times out */

	/* wait until disconnected */
	blz_loop_wait(ctx, &disconnect_noti, 10000);
	if (!disconnect_noti) {
		LOG_ERR("Timed out waiting for disconnection");
		return false;
	}

	/* free device and service structures (also frees char and
	 * unsubscribes notifications of bch) */
	blz_disconnect(dev);
	blz_serv_free(srv);
	srv = NULL;
	return true;
}

bool ble_connect_dfu_targ(const char* interface, const char* address,
						  enum BLE_ATYPE atype)
{
	/* connect to DfuTarg: increase MAC address by one */
	uint8_t* mac = blz_string_to_mac_s(address);
	mac[0]++;
	const char* macs = blz_mac_to_string_s(mac);

	LOG_NOTI("Connecting to DfuTarg (%s)...", macs);
	dev = retry_connect(macs, atype, CONNECT_DFUTARG_TRY);
	if (dev == NULL) {
		return false;
	}

	blz_set_connect_handler(dev, connect_handler, NULL);

	srv = blz_get_serv_from_uuid(dev, DFU_SERVICE_UUID);
	if (srv == NULL) {
		LOG_ERR("DFU Service not found");
		return false;
	}

	dp = blz_get_char_from_uuid(srv, DFU_DATA_UUID);
	cp = blz_get_char_from_uuid(srv, DFU_CONTROL_UUID);
	if (dp == NULL || cp == NULL) {
		LOG_ERR("Could not find DFU UUIDs");
		dp = cp = NULL;
		return false;
	}

	LOG_NOTI("DFU characteristics found");
	return start_cp_notify();
}

bool ble_write_ctrl(uint8_t* req, size_t len)
{
	if (conf.loglevel >= LL_DEBUG) {
		dump_data("CP: ", req, len);
	}
	return blz_char_write(cp, req, len);
}

bool ble_write_data(uint8_t* req, size_t len)
{
	if (conf.loglevel >= LL_DEBUG) {
		dump_data("TX: ", req, len);
	}
	return blz_char_write_cmd(dp, req, len);
}

const uint8_t* ble_read(void)
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

void ble_disconnect(void)
{
	if (cp) {
		blz_char_notify_stop(cp);
	}
	if (dev) {
		blz_disconnect(dev);
	}
}

void ble_fini(void)
{
	if (dev) {
		blz_disconnect(dev);
		dev = NULL;
	}
	if (srv) {
		blz_serv_free(srv); // also frees chars
		srv = NULL;
	}
	blz_fini(ctx);
	ctx = NULL;
	buttonless_noti = true;
	disconnect_noti = true;
	control_noti = true;
	terminate = true;
}

void ble_wait_disconnect(int ms)
{
	LOG_NOTI("Waiting for Bootloader to disconnect...");
	disconnect_noti = false;
	blz_loop_wait(ctx, &disconnect_noti, ms);
	if (!disconnect_noti) {
		LOG_ERR("Timed out waiting for disconnection");
	}
	/* necessary for blzlib (Bluez) */
	ble_disconnect();
}

#endif

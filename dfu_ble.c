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

#include <blzlib.h>
#include <blzlib_util.h>

#include "dfu_ble.h"
#include "log.h"
#include "conf.h"
#include "util.h"

#ifndef BLE_SUPPORT

bool ble_enter_dfu(const char *address) {}
bool ble_write_ctrl(uint8_t *req, size_t len) {}
bool ble_write_data(uint8_t *req, size_t len) {}
const uint8_t *ble_read(void) {}

#else

#define DFU_CONTROL_UUID "8EC90001-F315-4F60-9FB8-838830DAEA50"
#define DFU_DATA_UUID "8EC90002-F315-4F60-9FB8-838830DAEA50"
#define DFU_BUTTONLESS_UUID "8EC90003-F315-4F60-9FB8-838830DAEA50"
#define SERVICE_CHANGED_UUID "2A05"

static bool buttonless_noti = false;
static bool control_noti = false;
static blz *ctx;
static blz_char* cp = NULL;
static blz_char* dp = NULL;

static uint8_t recv_buf[200];

void buttonless_notify_handler(const uint8_t* data, size_t len, blz_char* ch)
{
    if (data[2] != 0x01) {
        LOG_ERR("Unexpected response (%zd) %x %x %x", len, data[0], data[1], data[2]);
    }
    buttonless_noti = true;
}

void control_notify_handler(const uint8_t* data, size_t len, blz_char* ch)
{
    memcpy(recv_buf, data, len);
    control_noti = true;

    if (conf.loglevel >= LL_DEBUG) {
        dump_data("RX: ", data, len);
    }
}

bool ble_enter_dfu(const char *address)
{
    ctx = blz_init("hci0");

    blz_dev *dev = blz_connect(ctx, address, NULL);
    if (dev == NULL) {
        LOG_ERR("could not connect");
        blz_fini(ctx);
    }

    blz_char *bch = blz_get_char_from_uuid(dev, DFU_BUTTONLESS_UUID);
    if (bch == NULL) {
        LOG_ERR("could not find UUID");
        blz_disconnect(dev);
        blz_fini(ctx);
    }

    blz_char_notify_start(bch, buttonless_notify_handler);

    uint8_t buf = 0x01;
    blz_char_write(bch, &buf, 1);
    /* wait until notification is received with confirmation */
    blz_loop_timeout(ctx, &buttonless_noti, 10000000);
    blz_disconnect(dev);

    // connect to DfuTarg

    // increase MAC address by one
    uint8_t* mac = blz_string_to_mac_s(address);
    mac[5]++;
    char macs[20];
    snprintf(macs, sizeof(macs), MAC_FMT, MAC_PAR(mac));
    LOG_INF("DFUTarg %s", macs);

    dev = blz_connect(ctx, macs, NULL);

    dp = blz_get_char_from_uuid(dev, DFU_DATA_UUID);
    cp = blz_get_char_from_uuid(dev, DFU_CONTROL_UUID);
    if (dp == NULL || cp == NULL) {
        LOG_ERR("could not find UUIDs");
        dp = cp = NULL;
        blz_disconnect(dev);
        blz_fini(ctx);
    }

    blz_char_notify_start(cp, control_notify_handler);
}

bool ble_write_ctrl(uint8_t *req, size_t len)
{
    blz_char_write(cp, req, len);
}

bool ble_write_data(uint8_t *req, size_t len)
{
    if (conf.loglevel >= LL_DEBUG) {
        dump_data("TX: ", req, len);
    }
    blz_char_write(dp, req, len);
}

const uint8_t *ble_read(void)
{
     /* wait until notification is received */
    control_noti = false;
    blz_loop_timeout(ctx, &control_noti, 10000000);
    return recv_buf;
}

#endif

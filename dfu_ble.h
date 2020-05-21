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

#ifndef DFU_BLE_H
#define DFU_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "conf.h"

bool ble_enter_dfu(const char* interface, const char* address,
				   enum BLE_ATYPE atype);
bool ble_write_ctrl(uint8_t* req, size_t len);
bool ble_write_data(uint8_t* req, size_t len);
const uint8_t* ble_read(void);
void ble_fini(void);

#endif

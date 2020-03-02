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

#ifndef DFU_H
#define DFU_H

#include <stdbool.h>
#include <stdint.h>
#include <zip.h>

bool dfu_ping(void);
bool dfu_set_packet_receive_notification(uint16_t prn);
bool dfu_get_serial_mtu(void);
uint32_t dfu_get_crc(void);
bool dfu_object_select(uint8_t type, uint32_t *offset, uint32_t *crc);
bool dfu_object_create(uint8_t type, uint32_t size);
bool dfu_object_write(zip_file_t *zf, size_t size);
bool dfu_object_execute(void);

bool dfu_object_write_procedure(uint8_t type, zip_file_t *zf, size_t sz);

#endif

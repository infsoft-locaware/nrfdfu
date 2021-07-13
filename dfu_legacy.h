/*
 * nrfdfu - Nordic DFU Upgrade Utility
 *
 * Copyright (C) 2021 Bruno Randolf (br1@einfach.org)
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

#ifndef LEGACY_DFU_H
#define LEGACY_DFU_H

#include <stdbool.h>
#include <stddef.h>
#include <zip.h>

#include "dfu.h"

enum dfu_ret ldfu_start(zip_file_t* sd_zip, size_t sd_size, zip_file_t* app_zip,
						size_t app_size);

#endif

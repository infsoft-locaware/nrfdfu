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

#define _GNU_SOURCE
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <json-c/json.h>
#include <zip.h>

#include "conf.h"
#include "dfu.h"
#include "dfu_ble.h"
#include "dfu_serial.h"
#include "log.h"
#include "serialtty.h"
#include "util.h"

struct config conf;

static struct option ser_options[] = {{"help", no_argument, NULL, 'h'},
									  {"verbose", optional_argument, NULL, 'v'},
									  {"port", required_argument, NULL, 'p'},
									  {"baud", required_argument, NULL, 'b'},
									  {"cmd", required_argument, NULL, 'c'},
									  {"hexcmd", required_argument, NULL, 'C'},
									  {"timeout", required_argument, NULL, 't'},
									  {NULL, 0, NULL, 0}};

static struct option ble_options[] = {{"help", no_argument, NULL, 'h'},
									  {"verbose", optional_argument, NULL, 'v'},
									  {"addr", required_argument, NULL, 'a'},
									  {"atype", optional_argument, NULL, 't'},
									  {"intf", optional_argument, NULL, 'i'},
									  {NULL, 0, NULL, 0}};

static void usage(void)
{
	fprintf(stderr,
			"Usage: nrfserdfu serial|ble [options] DFUPKG.zip\n"
			"Nordic NRF DFU Upgrade with DFUPKG.zip\n"
			"Options (all):\n"
			"  -h, --help\t\tShow help\n"
			"  -v, --verbose=<level>\tLog level 1 or 2 (-vv)\n"
			"\n"
			"Options (serial):\n"
			"  -p, --port <tty>\tSerial port (/dev/ttyUSB0)\n"
			"  -b, --baud <num>\tSerial baud rate (115200)\n"
			"  -c, --cmd <text>\tCommand to enter DFU mode\n"
			"  -C, --hexcmd <hex>\tCommand to enter DFU mode in HEX\n"
			"  -t, --timeout <num>\tTimeout after <num> tries (60)\n"
			"\n"
			"Options (BLE):\n"
			"  -a, --addr <mac>\tBLE MAC address to connect to\n"
			"  -t, --atype public|random\tBLE MAC address type (optional)\n"
			"  -i, --intf <name>\tBT interface name (hci0)\n");
}

static void main_options(int argc, char* argv[])
{
	/* defaults */
	conf.serport = "/dev/ttyUSB0";
	conf.serspeed = 115200;
	conf.loglevel = LL_NOTICE;
	conf.timeout = 10;
	conf.ble_atype = BAT_UNKNOWN;
	conf.interface = "hci0";
	conf.dfucmd_hex = false;

	if (argc <= 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	const char* type = argv[1];
	if (strncasecmp(argv[1], "ser", 3) == 0) {
		conf.dfu_type = DFU_SERIAL;
	} else if (strncasecmp(argv[1], "ble", 3) == 0) {
		conf.dfu_type = DFU_BLE;
#ifndef BLE_SUPPORT
		LOG_ERR("BLE Support is not compiled in");
		exit(EXIT_FAILURE);
#endif
	} else {
		LOG_ERR("Unknown DFU type %s", argv[1]);
		exit(EXIT_FAILURE);
		return;
	}

	int n = 0;
	while (n >= 0) {
		if (conf.dfu_type == DFU_SERIAL) {
			n = getopt_long(argc, argv, "hv::p:b:c:C:t:", ser_options, NULL);
		} else {
			n = getopt_long(argc, argv, "hv::a:t:i:", ble_options, NULL);
		}

		if (n < 0)
			continue;
		switch (n) {
		case '?':
			exit(EXIT_FAILURE);
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'v':
			if (optarg == NULL)
				conf.loglevel = LL_INFO;
			else if (optarg[0] == 'v' || optarg[0] == '2')
				conf.loglevel = LL_DEBUG;
			break;
		case 'p':
			conf.serport = optarg;
			break;
		case 'b':
			conf.serspeed = atoi(optarg);
			break;
		case 'c':
			conf.dfucmd = optarg;
			break;
		case 'C':
			conf.dfucmd = optarg;
			conf.dfucmd_hex = true;
			break;
		case 't':
			if (conf.dfu_type == DFU_SERIAL) {
				conf.timeout = atoi(optarg);
			} else {
				if (strncasecmp(optarg, "pub", 3) == 0) {
					conf.ble_atype = BAT_PUBLIC;
				} else if (strncasecmp(optarg, "rand", 4) == 0) {
					conf.ble_atype = BAT_RANDOM;
				}
			}
			break;
		case 'a':
			conf.ble_addr = optarg;
			break;
		case 'i':
			conf.interface = optarg;
			break;
		}
	}

	/* last non-option argument is ZIP file.
	 * attention: getopt reorders argv... even if "ble" or "ser" were argv[1]
	 * before it may now be last */
	if (argc > 2 && optind < argc && strcmp(argv[argc - 1], type) != 0) {
		conf.zipfile = argv[argc - 1];
	} else {
		LOG_ERR("ZIP file missing");
		exit(EXIT_FAILURE);
	}
}

static zip_file_t* zip_file_open(zip_t* zip, const char* name, size_t* size)
{
	struct zip_stat stat;

	zip_stat_init(&stat);
	int ret = zip_stat(zip, name, 0, &stat);
	if (ret < 0) {
		LOG_ERR("ZIP file does not contain %s", name);
		return NULL;
	}
	*size = stat.size;

	zip_file_t* zf = zip_fopen_index(zip, stat.index, 0);
	if (zf == NULL) {
		LOG_ERR("Error opening %s in ZIP file", name);
		return NULL;
	}
	return zf;
}

/* dat and bin have to be freed by caller */
static int read_manifest(zip_t* zip, char** dat, char** bin)
{
	char buf[400];

	zip_file_t* zf = zip_fopen(zip, "manifest.json", 0);
	if (zf == NULL) {
		LOG_ERR("ZIP file does not contain manifest");
		return -1;
	}

	zip_int64_t len = zip_fread(zf, buf, sizeof(buf));
	if (len <= 0) {
		LOG_ERR("Could not read Manifest");
		return -1;
	}

	/* read JSON */
	json_object* json;
	json_object* jobj;
	json_object* jobj2;
	enum json_tokener_error json_err;
	json = json_tokener_parse_verbose(buf, &json_err);
	if (json == NULL) {
		LOG_ERR("Manifest not valid JSON %d", json_err);
		zip_fclose(zf);
		return -1;
	}

	if (json_object_object_get_ex(json, "manifest", &jobj)
		&& json_object_object_get_ex(jobj, "application", &jobj2)) {
		if (json_object_object_get_ex(jobj2, "dat_file", &jobj)) {
			*dat = strdup(json_object_get_string(jobj));
		}
		if (json_object_object_get_ex(jobj2, "bin_file", &jobj)) {
			*bin = strdup(json_object_get_string(jobj));
		}
	}
	json_object_put(json);
	zip_fclose(zf);

	if (!*dat || !*bin) {
		LOG_ERR("Manifest format unknown");
		return -1;
	}

	return 0;
}

static void signal_handler(__attribute__((unused)) int signo)
{
	if (conf.dfu_type == DFU_SERIAL) {
		ser_fini();
	} else {
		ble_fini();
	}
}

int main(int argc, char* argv[])
{
	int ret = EXIT_FAILURE;
	char* dat = NULL;
	char* bin = NULL;
	zip_file_t* zf1 = NULL;
	zip_file_t* zf2 = NULL;
	size_t zs1, zs2;

	main_options(argc, argv);

	/* register the signal SIGINT handler */
	struct sigaction act;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	if (conf.dfu_type == DFU_SERIAL) {
		LOG_INF("Serial Port: %s (%d baud)", conf.serport, conf.serspeed);
	} else {
		if (conf.ble_addr == NULL) {
			LOG_ERR("Need BLE Target addr -a");
			exit(EXIT_FAILURE);
		}
		LOG_INF("BLE Target: %s", conf.ble_addr);
	}
	LOG_INF("DFU Package: %s", conf.zipfile);

	zip_t* zip = zip_open(conf.zipfile, ZIP_RDONLY, NULL);
	if (zip == NULL) {
		LOG_ERR("Could not open ZIP file '%s'", conf.zipfile);
		goto exit;
	}

	int man = read_manifest(zip, &dat, &bin);
	if (man < 0 || !dat || !bin) {
		goto exit;
	}

	/* open data files in ZIP file */
	zf1 = zip_file_open(zip, dat, &zs1);
	zf2 = zip_file_open(zip, bin, &zs2);
	if (zf1 == NULL || zf2 == NULL) {
		goto exit;
	}

	if (conf.dfu_type == DFU_SERIAL) {
		if (!ser_enter_dfu()) {
			goto exit;
		}
		if (!dfu_get_serial_mtu()) {
			goto exit;
		}
	} else {
		if (!ble_enter_dfu(conf.interface, conf.ble_addr, conf.ble_atype)) {
			goto exit;
		}

		dfu_set_mtu(244);
	}

	LOG_NOTI("Starting DFU upgrade");

	if (!dfu_set_packet_receive_notification(0)) {
		goto exit;
	}

	LOG_NOTI_("Sending Init: ");
	if (!dfu_object_write_procedure(1, zf1, zs1)) {
		goto exit;
	}
	LOG_NL(LL_NOTICE);

	LOG_NOTI_("Sending Firmware: ");
	if (!dfu_object_write_procedure(2, zf2, zs2)) {
		goto exit;
	}
	LOG_NL(LL_NOTICE);
	LOG_NOTI("Done");

	ret = EXIT_SUCCESS;

exit:
	free(bin);
	free(dat);
	if (zf1) {
		zip_fclose(zf1);
	}
	if (zf2) {
		zip_fclose(zf2);
	}
	if (zip) {
		zip_close(zip);
	}
	if (conf.dfu_type == DFU_SERIAL) {
		ser_fini();
	} else {
		ble_fini();
	}
	return ret;
}

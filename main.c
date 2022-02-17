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
#ifdef BLE_SUPPORT
#include "dfu_ble.h"
#endif
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

#ifdef BLE_SUPPORT
static struct option ble_options[] = {{"help", no_argument, NULL, 'h'},
									  {"verbose", optional_argument, NULL, 'v'},
									  {"addr", required_argument, NULL, 'a'},
									  {"atype", optional_argument, NULL, 't'},
									  {"intf", optional_argument, NULL, 'i'},
									  {"passkey", required_argument, NULL, 'p'},
									  {NULL, 0, NULL, 0}};
#endif

static void usage(void)
{
	fprintf(stderr,
#ifdef BLE_SUPPORT
			"Usage: nrfdfu serial|ble [options] DFUPKG.zip\n"
#else
			"Usage: nrfdfu serial [options] DFUPKG.zip\n"
#endif
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
#ifdef BLE_SUPPORT
			"\n"
			"Options (BLE):\n"
			"  -a, --addr <mac>\tBLE MAC address to connect to\n"
			"  -t, --atype public|random\tBLE MAC address type (optional)\n"
			"  -i, --intf <name>\tBT interface name (hci0)\n"
			"  -p, --passkey <6digits>\tUse BLE security with passkey\n"
#endif
	);
}

static void main_options(int argc, char* argv[])
{
	/* defaults */
	conf.serport = "/dev/ttyUSB0";
	conf.serspeed = 115200;
	conf.ser_acm = false;
	conf.loglevel = LL_NOTICE;
	conf.timeout = 10;
#ifdef BLE_SUPPORT
	conf.ble_atype = BAT_UNKNOWN;
	conf.interface = "hci0";
	conf.ble_passkey = NULL;
#endif
conf.dfucmd_hex = false;

	if (argc <= 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	const char* type = argv[1];
	if (strncasecmp(argv[1], "ser", 3) == 0) {
		conf.dfu_type = DFU_SERIAL;
#ifdef BLE_SUPPORT
	} else if (strncasecmp(argv[1], "ble", 3) == 0) {
		conf.dfu_type = DFU_BLE;
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
#ifdef BLE_SUPPORT
		} else {
			n = getopt_long(argc, argv, "hv::a:t:i:p:", ble_options, NULL);
#endif
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
			if (conf.dfu_type == DFU_SERIAL) {
				conf.serport = optarg;
				if (strstr(conf.serport, "ACM") != NULL) {
					conf.ser_acm = true;
				}
#ifdef BLE_SUPPORT
			} else {
				conf.ble_passkey = optarg;
#endif
			}
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
#ifdef BLE_SUPPORT
			} else {
				if (strncasecmp(optarg, "pub", 3) == 0) {
					conf.ble_atype = BAT_PUBLIC;
				} else if (strncasecmp(optarg, "rand", 4) == 0) {
					conf.ble_atype = BAT_RANDOM;
				}
#endif
			}
			break;
#ifdef BLE_SUPPORT
		case 'a':
			conf.ble_addr = optarg;
			break;
		case 'i':
			conf.interface = optarg;
			break;
		}
#endif
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

/* ap_dat and ap_bin have to be freed by caller */
static bool read_manifest(zip_t* zip, char** ap_dat, char** ap_bin,
						  char** sb_dat, char** sb_bin)
{
	bool ret = false;
	char buf[600];
	json_object* json = NULL;
	json_object* jobj;
	json_object* jobj2;
	json_object* jobj3;
	enum json_tokener_error json_err;

	zip_file_t* zf = zip_fopen(zip, "manifest.json", 0);
	if (zf == NULL) {
		LOG_ERR("ZIP file does not contain manifest");
		return false;
	}

	zip_int64_t len = zip_fread(zf, buf, sizeof(buf));
	if (len <= 0) {
		LOG_ERR("Could not read Manifest");
		goto exit;
	}

	/* read JSON */

	json = json_tokener_parse_verbose(buf, &json_err);
	if (json == NULL) {
		LOG_ERR("Manifest not valid JSON %d", json_err);
		goto exit;
	}

	if (!json_object_object_get_ex(json, "manifest", &jobj)) {
		LOG_ERR("Manifest format unknown");
		goto exit;
	}

	if (json_object_object_get_ex(jobj, "application", &jobj2)) {
		if (json_object_object_get_ex(jobj2, "dat_file", &jobj3)) {
			*ap_dat = strdup(json_object_get_string(jobj3));
		}
		if (json_object_object_get_ex(jobj2, "bin_file", &jobj3)) {
			*ap_bin = strdup(json_object_get_string(jobj3));
		}
		if (!*ap_dat || !*ap_bin) {
			LOG_ERR("Manifest missing app files");
			goto exit;
		}
	}

	if ((json_object_object_get_ex(jobj, "softdevice_bootloader", &jobj2)
		 || json_object_object_get_ex(jobj, "bootloader", &jobj2))) {
		if (json_object_object_get_ex(jobj2, "dat_file", &jobj3)) {
			*sb_dat = strdup(json_object_get_string(jobj3));
		}
		if (json_object_object_get_ex(jobj2, "bin_file", &jobj3)) {
			*sb_bin = strdup(json_object_get_string(jobj3));
		}
		if (!*sb_dat || !*sb_bin) {
			LOG_ERR("Manifest missing softdevice/bootloader files");
			goto exit;
		}
	}

	ret = true;

exit:
	json_object_put(json);
	// no need to json_object_put() the other jobs2/3
	zip_fclose(zf);
	return ret;
}

static void signal_handler(__attribute__((unused)) int signo)
{
	if (conf.dfu_type == DFU_SERIAL) {
		ser_fini();
#ifdef BLE_SUPPORT
	} else {
		ble_fini();
#endif
	}
}

int main(int argc, char* argv[])
{
	int ret = EXIT_FAILURE;
	char* ap_dat = NULL;
	char* ap_bin = NULL;
	char* sb_dat = NULL;
	char* sb_bin = NULL;
	zip_file_t* zf_ap_dat = NULL;
	zip_file_t* zf_ap_bin = NULL;
	zip_file_t* zf_sb_dat = NULL;
	zip_file_t* zf_sb_bin = NULL;
	size_t zs_ap_dat, zs_ap_bin, zs_sb_dat, zs_sb_bin;
	enum dfu_ret r;

	main_options(argc, argv);

	/* register the signal SIGINT handler */
	struct sigaction act;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	if (conf.dfu_type == DFU_SERIAL) {
		LOG_INF("Serial Port: %s (%d baud)", conf.serport, conf.serspeed);
#ifdef BLE_SUPPORT
	} else {
		if (conf.ble_addr == NULL) {
			LOG_ERR("Need BLE Target addr -a");
			exit(EXIT_FAILURE);
		}
		LOG_INF("BLE Target: %s", conf.ble_addr);
#endif
	}
	LOG_INF("DFU Package: %s", conf.zipfile);

	zip_t* zip = zip_open(conf.zipfile, ZIP_RDONLY, NULL);
	if (zip == NULL) {
		LOG_ERR("Could not open ZIP file '%s'", conf.zipfile);
		goto exit;
	}

	if (!read_manifest(zip, &ap_dat, &ap_bin, &sb_dat, &sb_bin)) {
		goto exit;
	}

	/* open all data files in ZIP file before starting */
	if (sb_dat && sb_bin) {
		zf_sb_dat = zip_file_open(zip, sb_dat, &zs_sb_dat);
		zf_sb_bin = zip_file_open(zip, sb_bin, &zs_sb_bin);
		if (zf_sb_dat == NULL || zf_sb_bin == NULL) {
			LOG_ERR("Cannot open SD files in ZIP");
			goto exit;
		}
		LOG_INF("Update contains Softdevice/Bootloader");
	}
	if (ap_dat && ap_bin) {
		zf_ap_dat = zip_file_open(zip, ap_dat, &zs_ap_dat);
		zf_ap_bin = zip_file_open(zip, ap_bin, &zs_ap_bin);
		if (zf_ap_dat == NULL || zf_ap_bin == NULL) {
			LOG_ERR("Cannot open APP files in ZIP");
			goto exit;
		}
		LOG_INF("Update contains Application");
	}

	if (sb_dat) {
		LOG_NOTI("Updating SoftDevice/Bootloader (%zd bytes):", zs_sb_bin);
	} else {
		LOG_NOTI("Updating Application (%zd bytes):", zs_ap_bin);
	}

	if (!dfu_bootloader_enter()) {
		goto exit;
	}

	if (sb_dat) {
		r = dfu_upgrade(zf_sb_dat, zs_sb_dat, zf_sb_bin, zs_sb_bin);
		if (r == DFU_RET_ERROR) {
			goto exit;
		} else if (r == DFU_RET_FW_VERSION) {
			/* Bootloader update may fail because it already has the same
			 * version. In this case try updating the Application */
			LOG_NOTI("SoftDevice/Bootloader not updated!");
			if (ap_dat) {
				LOG_NOTI("Updating Application (%zd bytes):", zs_ap_bin);
				goto update_app;
			}
		}
	}

	/* both updates BL+SD and APP are present, special handling of reconnection
	 * to BL after update */
	if (sb_dat && ap_dat) {
		LOG_NOTI("Updating Application (%zd bytes):", zs_ap_bin);
#ifdef BLE_SUPPORT
		if (conf.dfu_type == DFU_BLE) {
			/* wait until bootloader disconnect while updating BL+SD */
			ble_wait_disconnect(10000);
			/* connect to BL again */
			if (!ble_connect_dfu_targ(conf.interface, conf.ble_addr,
									  conf.ble_atype)) {
				/* if that fails, it may be that the APP is already running,
				 * try to connect normally */
				if (!dfu_bootloader_enter()) {
					goto exit;
				}
			}
		} else {
#endif
			/* Serial: Reopen ACM device and sleep a bit and then try to ping */
			if (conf.ser_acm) {
				ser_reopen(10);
			} else {
				sleep(5);
			}
			bool p = false;
			int cnt = 0;
			while (!p && cnt < 3) {
				p = dfu_ping();
				cnt++;
			}
#ifdef BLE_SUPPORT
		}
#endif
	}

update_app:
	if (ap_dat) {
		r = dfu_upgrade(zf_ap_dat, zs_ap_dat, zf_ap_bin, zs_ap_bin);
		if (r != DFU_RET_SUCCESS) {
			goto exit;
		}
	}

	ret = EXIT_SUCCESS;

exit:
	free(ap_bin);
	free(ap_dat);
	free(sb_bin);
	free(sb_dat);
	if (zf_ap_dat) {
		zip_fclose(zf_ap_dat);
	}
	if (zf_ap_bin) {
		zip_fclose(zf_ap_bin);
	}
	if (zf_sb_dat) {
		zip_fclose(zf_sb_dat);
	}
	if (zf_sb_bin) {
		zip_fclose(zf_sb_bin);
	}
	if (zip) {
		zip_close(zip);
	}
	if (conf.dfu_type == DFU_SERIAL) {
		ser_fini();
#ifdef BLE_SUPPORT
	} else {
		ble_fini();
#endif
	}
	return ret;
}

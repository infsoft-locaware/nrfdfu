#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zip.h>

#include "conf.h"
#include "log.h"
#include "serialtty.h"
#include "dfu.h"

struct config conf;
int ser_fd;

static struct option options[] = {
	{ "help",	no_argument,		NULL, 'h' },
	{ "debug",      optional_argument,	NULL, 'd' },
	{ "port",       required_argument,	NULL, 'p' },
	{ NULL, 0, NULL, 0 }
};

static void usage(void)
{
	fprintf(stderr, "nrfserdfu [options]\n"
			"options:\n"
			"[-h --help]\n"
			"[-d --debug]\n"
			"[-p --port]\n");
}

static void main_options(int argc, char* argv[])
{
	int n = 0;
	while (n >= 0) {
		n = getopt_long(argc, argv, "hd::p:", options, NULL);
		if (n < 0)
			continue;
		switch (n) {
		case 'h':
			usage();
			break;
		case 'd':
			if (optarg == NULL)
				conf.debug = 1;
			else
				conf.debug = atoi(optarg);
			break;
		case 'p':
			strncpy(conf.serport, optarg, CONF_MAX_LEN);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int ret;
	main_options(argc, argv);
	char buf[200];

	zip_t* zip = zip_open("/home/br1/ble-radar-0.0-35-gc6cff41-DFU.zip", ZIP_RDONLY, NULL);
	if (zip == NULL) {
		LOG_ERR("Could not open ZIP file");
		return EXIT_FAILURE;
	}

	zip_file_t* zf = zip_fopen(zip, "manifest.json", 0);
	if (zf == NULL) {
		LOG_ERR("Unexpected ZIP file contents");
		zip_close(zip);
		return EXIT_FAILURE;
	}
	zip_int64_t len = zip_fread(zf, buf, sizeof(buf));
	LOG_INF("MANI %s", buf);
	// TODO: read JSON

	struct zip_stat stat;
	zip_stat_init(&stat);
	ret = zip_stat(zip, "ble-radar-0.0-35-gc6cff41.dat", 0, &stat);
	if (ret < 0) {
		LOG_ERR("Unexpected ZIP file contents1");
		zip_close(zip);
		return EXIT_FAILURE;
	}
	size_t zs1 = stat.size;
	zip_file_t* zf1 = zip_fopen_index(zip, stat.index, 0);
	if (zf1 == NULL) {
		LOG_ERR("Error opening ZIP file");
		zip_close(zip);
		return EXIT_FAILURE;
	}

	zip_stat_init(&stat);
	ret = zip_stat(zip, "ble-radar-0.0-35-gc6cff41.bin", 0, &stat);
	if (ret < 0) {
		LOG_ERR("Unexpected ZIP file contents2");
		zip_close(zip);
		return EXIT_FAILURE;
	}
	size_t zs2 = stat.size;
	zip_file_t* zf2 = zip_fopen_index(zip, stat.index, 0);
	if (zf2 == NULL) {
		LOG_ERR("Error opening file");
		return EXIT_FAILURE;
	}

	LOG_INF("Port %s", conf.serport);
	ser_fd = serial_init(conf.serport);

	do {
		ret = dfu_ping();
		if (!ret)
			sleep(1);
	} while (!ret);

	dfu_set_packet_receive_notification(0);
	dfu_get_serial_mtu();

	dfu_object_write_procedure(1, zf1, zs1);
	dfu_object_write_procedure(2, zf2, zs2);

	zip_close(zip);
	serial_fini(ser_fd);
}

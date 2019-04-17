#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zip.h>
#include <json-c/json.h>

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

	/* first non-option argument is ZIP file */
	if (optind < argc) {
		conf.zipfile = argv[optind++];
	}

	if (optind < argc) {
		LOG_ERR("Garbage arguments from %s",argv[optind]);
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
void read_manifest(zip_t* zip, const char** dat, const char** bin)
{
	char buf[200];

	zip_file_t* zf = zip_fopen(zip, "manifest.json", 0);
	if (zf == NULL) {
		LOG_ERR("ZIP file does not contain manifest");
		return;
	}

	zip_int64_t len = zip_fread(zf, buf, sizeof(buf));
	if (len <= 0) {
		LOG_ERR("Could not read Manifest");
		return;
	}

	/* read JSON */
	json_object* json;
	json_object* jobj;
	json_object* jobj2;
	json = json_tokener_parse(buf);
	if (json == NULL) {
		LOG_ERR("Manifest not valid JSON");
		zip_fclose(zf);
		return;
	}

	if (json_object_object_get_ex(json, "manifest", &jobj) &&
	    json_object_object_get_ex(jobj, "application", &jobj2)) {
		if (json_object_object_get_ex(jobj2, "dat_file", &jobj)) {
			*dat = strdup(json_object_get_string(jobj));
		}
		if (json_object_object_get_ex(jobj2, "bin_file", &jobj)) {
			*bin = strdup(json_object_get_string(jobj));
		}
	}
	json_object_put(json);
	zip_fclose(zf);
}

int main(int argc, char *argv[])
{
	int ret;
	const char* dat = NULL;
	const char* bin = NULL;

	main_options(argc, argv);

	LOG_INF("Port %s", conf.serport);

	zip_t* zip = zip_open(conf.zipfile, ZIP_RDONLY, NULL);
	if (zip == NULL) {
		LOG_ERR("Could not open ZIP file");
		return EXIT_FAILURE;
	}

	read_manifest(zip, &dat, &bin);
	if (!dat || !bin) {
		LOG_ERR("Manifest format unknown");
		zip_close(zip);
		return EXIT_FAILURE;
	}

	/* open data files in ZIP file */
	size_t zs1, zs2;
	zip_file_t* zf1 = zip_file_open(zip, dat, &zs1);
	zip_file_t* zf2 = zip_file_open(zip, bin, &zs2);
	if (zf1 == NULL || zf2 == NULL) {
		zip_fclose(zf1);
		zip_fclose(zf2);
		zip_close(zip);
		return EXIT_FAILURE;
	}

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

	zip_fclose(zf1);
	zip_fclose(zf2);
	zip_close(zip);
	serial_fini(ser_fd);
}

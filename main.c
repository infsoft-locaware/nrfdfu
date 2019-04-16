#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "log.h"
#include "serialtty.h"

struct config conf;
static int ser_fd;

static struct option options[] = {
	{ "help",	no_argument,		NULL, 'h' },
	{ "debug",      no_argument,		NULL, 'd' },
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
		n = getopt_long(argc, argv, "hdp:", options, NULL);
		if (n < 0)
			continue;
		switch (n) {
		case 'h':
			usage();
			break;
		case 'd':
			conf.debug = true;
			break;
		case 'p':
			strncpy(conf.serport, optarg, CONF_MAX_LEN);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	main_options(argc, argv);

	LOG_INF("Port %s", conf.serport);
	ser_fd = serial_init(conf.serport);

	serial_fini(ser_fd);
}

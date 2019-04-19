#ifndef CONF_H_
#define CONF_H_

#include <stdbool.h>
#include <stdint.h>

#define CONF_MAX_LEN	200

struct config {
	int		loglevel;
	char*		serport;
	char*		zipfile;
	char*		dfucmd;
};

extern struct config conf;

#endif

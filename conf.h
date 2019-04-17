#ifndef CONF_H_
#define CONF_H_

#include <stdbool.h>
#include <stdint.h>

#define CONF_MAX_LEN	200

struct config {
	int		debug;
	char		serport[CONF_MAX_LEN];
};

extern struct config conf;

#endif

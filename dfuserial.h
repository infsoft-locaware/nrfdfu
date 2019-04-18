#ifndef DFUSER_H
#define DFUSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SLIP buffer size should be bigger than MTU */
#define BUF_SIZE	100
#define SLIP_BUF_SIZE	(BUF_SIZE * 2 + 1)

bool ser_encode_write(uint8_t* req, size_t len);
const char* ser_read_decode(void);

#endif

/*
 * Copyright (C) 2017 Bruno Randolf (br1@einfach.org)
 *
 * All rights granted to infsoft GmbH, Großmehring
 */

#ifndef LIBI_SERIALTTY_H_
#define LIBI_SERIALTTY_H_

#include <stdbool.h>

int serial_init(const char* device_name);
void serial_fini(int sock);
bool serial_wait_read_ready(int fd, int sec);
bool serial_wait_write_ready(int fd, int sec);
bool serial_write(int fd, const char* buf, size_t len, int timeout_sec);

#endif

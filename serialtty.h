/*
 * Copyright (C) 2017 Bruno Randolf (br1@einfach.org)
 *
 * All rights granted to infsoft GmbH, Großmehring
 */

#ifndef LIBI_SERIALTTY_H_
#define LIBI_SERIALTTY_H_

int serial_init(const char* device_name);
void serial_fini(int sock);

#endif

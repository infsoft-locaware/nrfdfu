#ifndef DFU_H
#define DFU_H

#include <stdbool.h>
#include <stdint.h>

bool dfu_ping(void);
bool dfu_set_packet_receive_notification(uint32_t prn);
bool dfu_get_serial_mtu(void);
bool dfu_read_object(uint32_t type);
bool dfu_create_object(uint32_t type, uint32_t size);

#endif

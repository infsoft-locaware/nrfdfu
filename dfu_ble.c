#include <blzlib.h>

#include "dfu_ble.h"
#include "log.h"

#define DFU_CONTROL_UUID "8EC90001-F315-4F60-9FB8-838830DAEA50"
#define DFU_DATA_UUID "8EC90002-F315-4F60-9FB8-838830DAEA50"
#define DFU_BUTTONLESS_UUID "8EC90003-F315-4F60-9FB8-838830DAEA50"
#define SERVICE_CHANGED_UUID "2A05"

bool ble_enter_dfu(const char *address)
{
    blz *blz = blz_init("hci0");

    blz_dev *dev = blz_connect(blz, address, NULL);
    if (dev == NULL) {
        LOG_ERR("could not connect");
        blz_fini(blz);
    }

    blz_char *bch = blz_get_char_from_uuid(dev, DFU_BUTTONLESS_UUID);
    if (bch == NULL) {
        LOG_ERR("could not find UUID");
        blz_disconnect(dev);
        blz_fini(blz);
    }
}

bool ble_write(uint8_t *req, size_t len)
{
}

const uint8_t *ble_read(void)
{
}

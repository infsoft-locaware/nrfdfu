#include "dfu_legacy.h"
#include "log.h"

// Legacy DFU
#define DFU_SERVICE_UUID	"00001530-1212-EFDE-1523-785FEABCD123"
#define DFU_CONTROL_UUID	"00001531-1212-EFDE-1523-785FEABCD123"
#define DFU_DATA_UUID		"00001532-1212-EFDE-1523-785FEABCD123"
#define DFU_BUTTONLESS_UUID "00001533-1212-EFDE-1523-785FEABCD123"

enum dfu_ret ldfu_start(zip_file_t* sd_zip, size_t sd_size, zip_file_t* app_zip,
						size_t app_size)
{
	LOG_NOTI("DFU LEGACY");
	return DFU_RET_SUCCESS;
}

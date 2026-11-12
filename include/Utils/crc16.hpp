#ifndef INC_UTILS_CRC16_HPP
#define INC_UTILS_CRC16_HPP

#include "LetoOSExport.h"

#include <stdint.h>
#include <stddef.h>

LETO_OS_EXPORT uint16_t calc_crc16(const void* buf, uint32_t len);

#endif 

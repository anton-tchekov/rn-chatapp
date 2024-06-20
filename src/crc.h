#ifndef __CRC_H__
#define __CRC_H__

#include "types.h"

u32 crc_update(u32 crc, const u8 *buf, size_t len);
void crc_test(void);

#endif

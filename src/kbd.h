#ifndef __KBD_H__
#define __KBD_H__

#include "types.h"

#define MOD_SHIFT          0x8000
#define MOD_CTRL           0x4000
#define MOD_OS             0x2000
#define MOD_ALT            0x1000
#define MOD_ALT_GR         0x800

enum
{
	KEYSTATE_RELEASED,
	KEYSTATE_PRESSED,
	KEYSTATE_REPEAT
};

i32 key_convert(i32 scancode, i32 mod);
i32 key_to_codepoint(i32 k);

#endif

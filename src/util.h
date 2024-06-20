#ifndef __UTIL_H__
#define __UTIL_H__

#include "types.h"

#define ARRLEN(X) (sizeof(X) / sizeof(*X))

#define GENERATE_ENUM(ENUM) ENUM
#define GENERATE_STRING(STRING) #STRING

void *smalloc(size_t size);
void *scalloc(size_t size);
void sfree(void *p);
void print_allocs(void);
size_t filter(void *base, size_t num, size_t width, const void *data,
	int (*keep)(void *elem, const void *data));

#endif

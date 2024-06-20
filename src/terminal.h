#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include "types.h"
#include <stdarg.h>

enum
{
	TAG_LOG,
	TAG_ACK,
	TAG_NACK,
	TAG_RECV
};

typedef struct
{
	uint32_t tag;
	size_t len;
	char text[];
} TLine;

typedef struct
{
	size_t num_lines;
	TLine **lines;
} Terminal;

void term_init(Terminal *term, size_t cap);
void term_draw(Terminal *term, i32 x, i32 y, i32 max_y);
void term_print(Terminal *term, uint32_t tag, const char *txt, ...);
void term_print_va(Terminal *term, uint32_t tag, const char *txt, va_list args);
void term_clear(Terminal *term);
void term_free(Terminal *term);
TLine *term_line(Terminal *term, size_t i);

#endif

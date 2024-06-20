#include "terminal.h"
#include "util.h"
#include "gfx.h"
#include "gui.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void term_init(Terminal *term, size_t cap)
{
	term->num_lines = cap;
	term->lines = scalloc(cap * sizeof(*term->lines));
}

static uint32_t tag_color(uint32_t tag)
{
	uint32_t color = 0xFFFFFF;
	switch(tag)
	{
	case TAG_ACK:
		color = 0x00FF00;
		break;
	case TAG_NACK:
		color = 0xFF0000;
		break;
	case TAG_RECV:
		color = 0xFF8000;
		break;
	default:
		break;
	}

	return color;
}

void term_draw(Terminal *term, i32 x, i32 y, i32 max_y)
{
	size_t i = 0;
	TLine **lines = term->lines;
	while(y > max_y && i < term->num_lines)
	{
		TLine *line = lines[i];
		y -= FONT_HEIGHT + 8;
		if(!line) { return; }
		gfx_rect(x, y + 8, 10, 10, tag_color(line->tag));
		font_string_len(x + 18, y, line->text, line->len, 0, 0);
		++i;
	}
}

TLine *term_line(Terminal *term, size_t i)
{
	TLine **lines = term->lines;
	return lines[i];
}

void term_print_va(Terminal *term, uint32_t tag, const char *txt, va_list args)
{
	size_t len;
	TLine *line, **lines;
	va_list args2;

	va_copy(args2, args);
	len = vsnprintf(NULL, 0, txt, args) + 1;
	line = smalloc(sizeof(TLine) + len);
	line->len = len;
	line->tag = tag;
	vsnprintf(line->text, len, txt, args2);
	va_end(args2);

	lines = term->lines;
	sfree(lines[term->num_lines - 1]);
	memmove(lines + 1, lines, (term->num_lines - 1) * sizeof(*lines));
	lines[0] = line;
}

void term_print(Terminal *term, uint32_t tag, const char *txt, ...)
{
	va_list args;
	va_start(args, txt);
	term_print_va(term, tag, txt, args);
	va_end(args);
}

void term_clear(Terminal *term)
{
	TLine **lines = term->lines;
	for(size_t i = 0; i < term->num_lines; ++i)
	{
		sfree(lines[i]);
		lines[i] = NULL;
	}
}

void term_free(Terminal *term)
{
	term_clear(term);
	sfree(term->lines);
}

#ifndef __GFX_H__
#define __GFX_H__

#include "types.h"

#define FONT_WIDTH  16
#define FONT_HEIGHT 29

extern int width, height;

void gfx_destroy(void);
void gfx_init(void);
int font_string_len(int x, int y, const char *s, int len, u32 bg, u32 fg);
int font_string(int x, int y, const char *s, u32 bg, u32 fg);
int font_string_width(const char *s);
int font_string_width_len(const char *text, int len);
void gfx_clear(u32 color);
void gfx_rect(int x, int y, int w, int h, u32 color);
void gfx_rect_border(i32 x, i32 y, i32 w, i32 h, i32 border, u32 color);
void gfx_update(void);
void gfx_notify(void);
void gfx_set_title(const char *title);

#endif

#ifndef __GUI_H__
#define __GUI_H__

#include "types.h"

#define ALIGN_MASK            0x03
#define FLAG_ALIGN_LEFT       0x00
#define FLAG_ALIGN_CENTER     0x01
#define FLAG_ALIGN_RIGHT      0x02
#define FLAG_PASSWORD         0x10
#define FLAG_INVERTED         0x20
#define FLAG_INVISIBLE        0x40

#define INPUT_HEIGHT          (17 + FONT_HEIGHT)

enum
{
	ELEMENT_TYPE_BUTTON,
	ELEMENT_TYPE_INPUT,
	ELEMENT_TYPE_LABEL
};

typedef struct
{
	u32 Type;
	u32 Flags;
} Element;

typedef struct
{
	u32 Type;
	u32 Flags;
	i32 X;
	i32 Y;
	char *Text;
} Label;

typedef struct
{
	u32 Type;
	u32 Flags;
	u32 Tag;
	i32 X;
	i32 Y;
	i32 W;
	i32 H;
	char *Text;
	void (*Click)(Element *);
} Button;

typedef struct
{
	u32 Type;
	u32 Flags;
	i32 X;
	i32 Y;
	i32 W;
	i32 Position;
	i32 Selection;
	i32 Length;
	char *Text;
	void (*Enter)(Element *);
} Input;

typedef struct
{
	u32 ColorFG;
	u32 ColorBG;
	u32 ColorTextSelBG;
	u32 ColorTextSelFG;
	u32 ColorBorder;
	u32 ColorBorderSel;
	u32 ElementBG;
	u32 ElementSelBG;
	u32 ColorCursor;
} Theme;

typedef struct
{
	void **Elements;
	i32 Count;
	i32 Selected;
	i32 Hover;
} Window;

void window_open(Window *window);
void gui_render(void);
void gui_event_key(u32 key, u32 chr, u32 state);
void gui_mouseup(i32 x, i32 y);
void gui_mousedown(i32 x, i32 y);
void gui_mousemove(i32 x, i32 y);
void input_clear(Input *i);

#endif

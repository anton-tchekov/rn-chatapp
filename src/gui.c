#include "gui.h"
#include "kbd.h"
#include "gfx.h"
#include <ctype.h>
#include <string.h>
#include <SDL2/SDL.h>

#define FLAG_SELECTED         0x100
#define FLAG_HOVER            0x200
#define FLAG_ACTIVE           0x400

/* --- Default Theme --- */
#define THEME_BLACK           0x100000
#define THEME_DARK            0x310000
#define THEME_MIDDLE          0x7b0000
#define THEME_LIGHT           0xff8200

#define BORDER_SIZE          2
#define BORDER_SIZE_SEL      2
#define INPUT_PADDING_X      8
#define INPUT_PADDING_Y      8
#define CURSOR_WIDTH         2
#define CURSOR_HEIGHT         (4 + FONT_HEIGHT)
#define CURSOR_OFFSET_X      0
#define CURSOR_OFFSET_Y      6
#define LABEL_Y_PADDING      3
#define LABEL_X_PADDING     10

static Window *window_cur;

static Theme theme_default =
{
	.ColorFG = THEME_LIGHT,
	.ColorBG = THEME_BLACK,
	.ColorTextSelBG = THEME_LIGHT,
	.ColorTextSelFG = THEME_DARK,
	.ColorBorder = THEME_MIDDLE,
	.ColorBorderSel = THEME_LIGHT,
	.ElementBG = THEME_DARK,
	.ElementSelBG = THEME_MIDDLE,
	.ColorCursor = THEME_LIGHT,
};

static Theme *theme_cur = &theme_default;

i32 i32_min(i32 a, i32 b)
{
	return a < b ? a : b;
}

i32 i32_max(i32 a, i32 b)
{
	return a > b ? a : b;
}

static void input_replace(Input *input, u32 index, u32 count,
	const void *elems, u32 new_count)
{
	u32 new_length, last_bytes;

	new_length = input->Length - count + new_count;
	last_bytes = (input->Length - index - count);

	memmove(input->Text + index + new_count,
		input->Text + index + count,
		last_bytes);

	memcpy(input->Text + index, elems, new_count);

	input->Length = new_length;
}

static inline void input_remove(Input *input, u32 index)
{
	input_replace(input, index, 1, NULL, 0);
}

static void label_render(Label *l)
{
	if(l->Flags & FLAG_INVISIBLE)
	{
		return;
	}

	u32 align, w = 0, y, rx = 0, h = 12 + 2 * LABEL_Y_PADDING;
	u32 fg, bg;

	y = l->Y;
	align = l->Flags & ALIGN_MASK;

	if(l->Flags & FLAG_INVERTED)
	{
		fg = theme_cur->ColorBG;
		bg = theme_cur->ColorFG;
	}
	else
	{
		fg = theme_cur->ColorFG;
		bg = theme_cur->ColorBG;
	}

	if(align == FLAG_ALIGN_CENTER)
	{
		w = font_string_width(l->Text) + 2 * LABEL_X_PADDING;
		rx = w / 2;
	}
	else if(align == FLAG_ALIGN_RIGHT)
	{
		w = font_string_width(l->Text) + 2 * LABEL_X_PADDING;
		rx = w;
	}
	else if(align == FLAG_ALIGN_LEFT)
	{
		if(l->Flags & FLAG_INVERTED)
		{
			w = font_string_width(l->Text) + 2 * LABEL_X_PADDING;
		}
	}

	if(l->Flags & FLAG_INVERTED)
	{
		if(l->Flags & FLAG_INVISIBLE)
		{
			gfx_rect(l->X - rx - LABEL_X_PADDING, y - LABEL_Y_PADDING, w, h,
				theme_cur->ColorBG);
			return;
		}
		else
		{
			gfx_rect(l->X - rx - LABEL_X_PADDING, y - LABEL_Y_PADDING, w, h, bg);
		}
	}

	font_string(l->X - rx, y, l->Text, fg, bg);
}

static void button_render(Button *b, u32 flags)
{
	u32 inner_color;
	i32 x, y;

	if(b->Flags & FLAG_INVISIBLE)
	{
		return;
	}

	y = b->Y;

	inner_color = (flags & (FLAG_SELECTED | FLAG_HOVER)) ?
		theme_cur->ElementSelBG : theme_cur->ElementBG;

	gfx_rect(b->X, y, b->W, b->H, inner_color);

	if(flags & FLAG_SELECTED)
	{
		gfx_rect_border(b->X, y, b->W, b->H, BORDER_SIZE_SEL,
			theme_cur->ColorBorderSel);
	}
	else
	{
		gfx_rect_border(b->X, y, b->W, b->H, BORDER_SIZE,
			theme_cur->ColorBorder);
	}

	x = b->X + b->W / 2 - font_string_width(b->Text) / 2;
	font_string(x, y + b->H / 2 - FONT_HEIGHT / 2,
		b->Text,
		theme_cur->ColorFG,
		inner_color);
}

static const char *input_get_display_text(Input *i)
{
	static const char _password_stars[] = "***********************************";
	return i->Flags & FLAG_PASSWORD ? _password_stars : i->Text;
}

static void input_render(Input *i, u32 flags)
{
	if(i->Flags & FLAG_INVISIBLE)
	{
		return;
	}

	u32 inner_color;
	i32 y = i->Y;

	const char *text = input_get_display_text(i);

	inner_color = (flags & (FLAG_SELECTED | FLAG_HOVER)) ?
		theme_cur->ElementSelBG : theme_cur->ElementBG;

	gfx_rect(i->X + BORDER_SIZE,
		i->Y + BORDER_SIZE,
		i->W - 2 * BORDER_SIZE,
		INPUT_HEIGHT - 2 * BORDER_SIZE,
		inner_color);

	if(flags & FLAG_SELECTED)
	{
		gfx_rect_border(i->X, i->Y,
			i->W, INPUT_HEIGHT, BORDER_SIZE_SEL,
			theme_cur->ColorBorderSel);
	}
	else
	{
		gfx_rect_border(i->X, y, i->W, INPUT_HEIGHT, BORDER_SIZE,
			theme_cur->ColorBorder);
	}

	if(i->Selection == i->Position)
	{
		font_string_len(i->X + INPUT_PADDING_X, y + INPUT_PADDING_Y,
			text, i->Length,
			theme_cur->ColorFG,
			inner_color);
	}
	else
	{
		i32 sel_start, sel_len, sel_x;

		sel_start = i32_min(i->Selection, i->Position);
		sel_len = i32_max(i->Selection, i->Position) - sel_start;

		font_string_len(i->X + INPUT_PADDING_X,
			y + INPUT_PADDING_Y,
			text, sel_start,
			theme_cur->ColorFG,
			inner_color);

		sel_x = font_string_width_len(text, sel_start);

		gfx_rect(i->X + INPUT_PADDING_X + sel_x, y + CURSOR_OFFSET_Y,
			font_string_width_len(text + sel_start, sel_len),
			CURSOR_HEIGHT,
			theme_cur->ColorFG);

		font_string_len(i->X + INPUT_PADDING_X + sel_x, y + INPUT_PADDING_Y,
			text + sel_start, sel_len,
			theme_cur->ColorTextSelFG,
			theme_cur->ColorTextSelBG);

		font_string_len(i->X + INPUT_PADDING_X +
			font_string_width_len(text, sel_start + sel_len),
			y + INPUT_PADDING_Y,
			text + sel_start + sel_len,
			i->Length - sel_start - sel_len,
			theme_cur->ColorFG,
			inner_color);
	}

	if(flags & FLAG_SELECTED)
	{
		gfx_rect(i->X + INPUT_PADDING_X +
			font_string_width_len(text, i->Position) +
			CURSOR_OFFSET_X,
			i->Y + CURSOR_OFFSET_Y,
			CURSOR_WIDTH,
			CURSOR_HEIGHT,
			theme_cur->ColorCursor);
	}
}

void input_clear(Input *i)
{
	i->Length = 0;
	i->Selection = 0;
	i->Position = 0;
}

static void input_selection_replace(Input *i, const char *str, i32 len)
{
	const char *text;
	i32 sel_start, sel_len, w_new, w_start, w_end;

	sel_start = i32_min(i->Selection, i->Position);
	sel_len = i32_max(i->Selection, i->Position) - sel_start;

	text = input_get_display_text(i);

	w_new = font_string_width_len(str, len);
	w_start = font_string_width_len(text, sel_start);
	w_end = font_string_width_len(text + sel_start + sel_len,
		i->Length - sel_start - sel_len);

	if(w_new + w_start + w_end >= i->W - 2 * INPUT_PADDING_X)
	{
		return;
	}

	input_replace(i, sel_start, sel_len, str, len);
	i->Position = sel_start + len;
	i->Selection = i->Position;
}

static void input_left(Input *i)
{
	if(i->Selection != i->Position)
	{
		i->Selection = i->Position;
	}
	else if(i->Position > 0)
	{
		--i->Position;
		i->Selection = i->Position;
	}
}

static void input_select_left(Input *i)
{
	if(i->Position > 0)
	{
		--i->Position;
	}
}

static void input_right(Input *i)
{
	if(i->Selection != i->Position)
	{
		i->Selection = i->Position;
	}
	else if(i->Position < (i32)i->Length)
	{
		++i->Position;
		i->Selection = i->Position;
	}
}

static void input_select_right(Input *i)
{
	if(i->Position < (i32)i->Length)
	{
		++i->Position;
	}
}

static void input_backspace(Input *i)
{
	if(i->Selection != i->Position)
	{
		input_selection_replace(i, NULL, 0);
	}
	else if(i->Position > 0)
	{
		--i->Position;
		i->Selection = i->Position;
		input_remove(i, i->Position);
	}
}

static void input_delete(Input *i)
{
	if(i->Selection != i->Position)
	{
		input_selection_replace(i, NULL, 0);
	}
	else if(i->Position < (i32)i->Length)
	{
		input_remove(i, i->Position);
	}
}

static void input_home(Input *i)
{
	i->Selection = 0;
	i->Position = 0;
}

static void input_select_home(Input *i)
{
	i->Position = 0;
}

static void input_end(Input *i)
{
	i->Position = i->Length;
	i->Selection = i->Position;
}

static void input_select_end(Input *i)
{
	i->Position = i->Length;
}

static void input_char(Input *i, u32 chr)
{
	char ins = chr;
	input_selection_replace(i, &ins, 1);
}

static void input_select_all(Input *i)
{
	i->Selection = 0;
	i->Position = i->Length;
}

static void _selection_save(Input *i)
{
	int chr;
	char *p;
	i32 sel_start, sel_len;

	sel_start = i32_min(i->Selection, i->Position);
	sel_len = i32_max(i->Selection, i->Position) - sel_start;
	p = i->Text + sel_start + sel_len;
	chr = *p;
	*p = '\0';
	SDL_SetClipboardText(i->Text + sel_start);
	*p = chr;
}

static void input_copy(Input *i)
{
	_selection_save(i);
}

static void input_cut(Input *i)
{
	_selection_save(i);
	input_selection_replace(i, NULL, 0);
}

static void input_paste(Input *i)
{
	char *p = SDL_GetClipboardText();
	input_selection_replace(i, p, strlen(p));
	free(p);
}

static void input_click(Input *i, i32 x)
{
	i32 offset = x - i->X - INPUT_PADDING_X;
	if(offset < 0) { return; }
	offset = (offset + (FONT_WIDTH / 2)) / FONT_WIDTH;
	if(offset > i->Length)
	{
		offset = i->Length;
	}

	i->Position = offset;
	i->Selection = offset;
}

static void input_event_key(Input *i, u32 key, u32 chr)
{
	u32 nomods = key & 0xFF;
	if(key == SDL_SCANCODE_HOME)
	{
		input_home(i);
	}
	else if(key == (SDL_SCANCODE_HOME | MOD_SHIFT))
	{
		input_select_home(i);
	}
	else if(key == SDL_SCANCODE_END)
	{
		input_end(i);
	}
	else if(key == (SDL_SCANCODE_END | MOD_SHIFT))
	{
		input_select_end(i);
	}
	else if(key == SDL_SCANCODE_LEFT)
	{
		input_left(i);
	}
	else if(key == (SDL_SCANCODE_LEFT | MOD_SHIFT))
	{
		input_select_left(i);
	}
	else if(key == SDL_SCANCODE_RIGHT)
	{
		input_right(i);
	}
	else if(key == (SDL_SCANCODE_RIGHT | MOD_SHIFT))
	{
		input_select_right(i);
	}
	else if(nomods == SDL_SCANCODE_BACKSPACE)
	{
		input_backspace(i);
	}
	else if(nomods == SDL_SCANCODE_DELETE)
	{
		input_delete(i);
	}
	else if(key == (SDL_SCANCODE_A | MOD_CTRL))
	{
		input_select_all(i);
	}
	else if(key == (SDL_SCANCODE_C | MOD_CTRL))
	{
		input_copy(i);
	}
	else if(key == (SDL_SCANCODE_X | MOD_CTRL))
	{
		input_cut(i);
	}
	else if(key == (SDL_SCANCODE_V | MOD_CTRL))
	{
		input_paste(i);
	}
	else if(nomods == SDL_SCANCODE_RETURN)
	{
		if(i->Enter)
		{
			i->Enter((Element *)i);
		}
	}
	else if(isprint(chr))
	{
		input_char(i, chr);
	}
}

static void element_render(Element *e, u32 flags)
{
	switch(e->Type)
	{
	case ELEMENT_TYPE_LABEL:
		label_render((Label *)e);
		break;

	case ELEMENT_TYPE_BUTTON:
		button_render((Button *)e, flags);
		break;

	case ELEMENT_TYPE_INPUT:
		input_render((Input *)e, flags);
		break;
	}
}

static u32 _selectable(i32 index)
{
	u32 type = ((Element *)window_cur->Elements[index])->Type;
	return type == ELEMENT_TYPE_BUTTON ||
		type == ELEMENT_TYPE_INPUT;
}

static void _select(i32 index)
{
	window_cur->Selected = index;
}

static void element_first(void)
{
	i32 i, count = window_cur->Count;
	for(i = 0; i < count; ++i)
	{
		if(_selectable(i))
		{
			window_cur->Selected = i;
			break;
		}
	}
}

static void element_next(void)
{
	i32 i, count = window_cur->Count;
	for(i = window_cur->Selected + 1; i < count; ++i)
	{
		if(_selectable(i))
		{
			_select(i);
			return;
		}
	}

	for(i = 0; i < count; ++i)
	{
		if(_selectable(i))
		{
			_select(i);
			return;
		}
	}
}

static void element_prev(void)
{
	i32 i;
	for(i = window_cur->Selected - 1; i >= 0; --i)
	{
		if(_selectable(i))
		{
			_select(i);
			return;
		}
	}

	for(i = window_cur->Count - 1; i >= 0; --i)
	{
		if(_selectable(i))
		{
			_select(i);
			return;
		}
	}
}

void gui_render(void)
{
	gfx_clear(theme_cur->ColorBG);
	for(i32 i = 0; i < window_cur->Count; ++i)
	{
		u32 flags = 0;
		if(window_cur->Selected == i) { flags |= FLAG_SELECTED; }
		if(window_cur->Hover == i) { flags |= FLAG_HOVER; }
		element_render(window_cur->Elements[i], flags);
	}
}

static int input_bounds(Input *e, i32 x, i32 y)
{
	if(e->Flags & FLAG_INVISIBLE)
	{
		return 0;
	}

	return x >= e->X && y >= e->Y && x < e->X + e->W && y < e->Y + INPUT_HEIGHT;
}

static int button_bounds(Button *e, i32 x, i32 y)
{
	if(e->Flags & FLAG_INVISIBLE)
	{
		return 0;
	}

	return x >= e->X && y >= e->Y && x < e->X + e->W && y < e->Y + e->H;
}

static int element_hover(Element *e, i32 x, i32 y)
{
	switch(e->Type)
	{
	case ELEMENT_TYPE_BUTTON:
		return button_bounds((Button *)e, x, y);

	case ELEMENT_TYPE_INPUT:
		return input_bounds((Input *)e, x, y);

	default:
		break;
	}

	return 0;
}

void window_open(Window *window)
{
	window_cur = window;
	if(window->Selected < 0)
	{
		element_first();
	}
}

void gui_mousemove(i32 x, i32 y)
{
	i32 i;
	for(i = 0; i < window_cur->Count; ++i)
	{
		if(element_hover(window_cur->Elements[i], x, y))
		{
			window_cur->Hover = i;
			return;
		}
	}

	window_cur->Hover = -1;
}

void gui_mouseup(i32 x, i32 y)
{
	gui_mousemove(x, y);
}

void gui_mousedown(i32 x, i32 y)
{
	gui_mousemove(x, y);
	window_cur->Selected = window_cur->Hover;
	if(window_cur->Selected < 0)
	{
		return;
	}

	Element *ce = window_cur->Elements[window_cur->Selected];
	u32 type = ce->Type;
	if(type == ELEMENT_TYPE_BUTTON)
	{
		Button *b = (Button *)ce;
		if(b->Click)
		{
			b->Click(ce);
		}
	}
	else if(type == ELEMENT_TYPE_INPUT)
	{
		input_click((Input *)ce, x);
	}
}

void gui_event_key(u32 key, u32 chr, u32 state)
{
	void *ce;
	if(!window_cur)
	{
		return;
	}

	if(window_cur->Selected < 0)
	{
		return;
	}

	if(state == KEYSTATE_RELEASED)
	{
		return;
	}

	ce = window_cur->Elements[window_cur->Selected];
	if(key == SDL_SCANCODE_TAB)
	{
		element_next();
	}
	else if(key == (SDL_SCANCODE_TAB | MOD_SHIFT))
	{
		element_prev();
	}
	else
	{
		u32 type = ((Element *)ce)->Type;
		if(type == ELEMENT_TYPE_BUTTON)
		{
			if(key == SDL_SCANCODE_RETURN)
			{
				Button *b = ce;
				if(b->Click)
				{
					b->Click(ce);
				}
			}
		}
		else if(type == ELEMENT_TYPE_INPUT)
		{
			input_event_key((Input *)ce, key, chr);
		}
	}
}

#if 0
static void theme_set(Theme *theme)
{
	theme_cur = theme;
}

static void label_show(Label *label, u32 show)
{
	if((show && (label->Flags & FLAG_INVISIBLE)) ||
		(!show && !(label->Flags & FLAG_INVISIBLE)))
	{
		label->Flags ^= FLAG_INVISIBLE;
	}
}
#endif

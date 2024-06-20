#include "gfx.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

int width = 1030;
int height = 610;

static u32 gfx_notify_event;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

void gfx_destroy(void)
{
	if(texture)
	{
		SDL_DestroyTexture(texture);
		texture = NULL;
	}

	if(renderer)
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	if(window)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}

	if(SDL_WasInit(SDL_INIT_VIDEO))
	{
		SDL_Quit();
	}
}

void gfx_init(void)
{
	if(SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Error initializing SDL; SDL_Init: %s\n", SDL_GetError());
		gfx_destroy();
		exit(1);
	}

	if(!(window = SDL_CreateWindow("RN ChatApp",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height, SDL_WINDOW_RESIZABLE)))
	{
		printf("Error creating SDL_Window: %s\n", SDL_GetError());
		gfx_destroy();
		exit(1);
	}

	SDL_SetWindowMinimumSize(window, width, height);

	if(!(renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)))
	{
		printf("Error creating SDL_Renderer: %s\n", SDL_GetError());
		gfx_destroy();
		exit(1);
	}

	if(!(texture = IMG_LoadTexture(renderer, "font_big.png")))
	{
		printf("Error creating SDL_Texture: %s\n", SDL_GetError());
		gfx_destroy();
		exit(1);
	}

	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	gfx_notify_event = SDL_RegisterEvents(1);
}

void gfx_set_title(const char *title)
{
	SDL_SetWindowTitle(window, title);
}

static void set_color(u32 color)
{
	SDL_SetRenderDrawColor(renderer,
		(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
}

int font_string_len(int x, int y, const char *s, int len, u32 bg, u32 fg)
{
	int i;
	int w = 0;
	set_color(fg);
	for(i = 0; i < len && *s; ++i, ++s, w += FONT_WIDTH)
	{
		int c = *s;
		if(c < 32 || c > 126)
		{
			c = '?';
		}

		c -= 32;

		SDL_Rect src = { c * FONT_WIDTH, 0, FONT_WIDTH, FONT_HEIGHT };
		SDL_Rect dst = { x + w, y, FONT_WIDTH, FONT_HEIGHT };
		SDL_RenderCopy(renderer, texture, &src, &dst);
	}

	return w;
	(void)bg;
}

int font_string(int x, int y, const char *s, u32 bg, u32 fg)
{
	return font_string_len(x, y, s, 0xFFFF, bg, fg);
}

int font_string_width(const char *s)
{
	return strlen(s) * FONT_WIDTH;
}

int font_string_width_len(const char *text, int len)
{
	int i, w = 0;
	for(i = 0; i < len && *text; ++i) { w += FONT_WIDTH; }
	return w;
}

void gfx_clear(u32 color)
{
	set_color(color);
	SDL_RenderClear(renderer);
}

void gfx_rect(int x, int y, int w, int h, u32 color)
{
	SDL_Rect rect = { x, y, w, h };
	set_color(color);
	SDL_RenderFillRect(renderer, &rect);
}

void gfx_rect_border(i32 x, i32 y, i32 w, i32 h, i32 border, u32 color)
{
	gfx_rect(x, y, w, border, color);
	gfx_rect(x, y + h - border, w, border, color);

	gfx_rect(x, y + border, border, h - 2 * border, color);
	gfx_rect(x + w - border, y + border, border, h - 2 * border, color);
}

void gfx_notify(void)
{
	SDL_Event e;
	e.type = gfx_notify_event;
	SDL_PushEvent(&e);
}

void gfx_update(void)
{
	SDL_RenderPresent(renderer);
}

#pragma once

#include <3ds.h>

#define MAX_SELECTED_OPTIONS 0x10

#define COLOR_TITLE 0x0000FF
#define COLOR_NEUTRAL 0xFFFFFF
#define COLOR_SELECTED 0xFF0000
#define COLOR_BACKGROUND 0x000000

#define CONSOLE_REVERSE		CONSOLE_ESC(7m)

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ConsoleMenu {
	PrintConsole menuConsole;
} ConsoleMenu;

ConsoleMenu currentMenu;
void init_menu(gfxScreen_t screen);
void menu_draw_string(const char* str, int pos_x, int pos_y, const char* color);
void menu_draw_string_full(const char* str, int pos_y, const char* color);
void menu_multkey_draw(const char *title, const char* footer, int back, int count, const char *options[], void* data,
                       bool (*callback)(int result, u32 key, void* data));
int *menu_draw_selection(const char *title, int count, const char *options[], const int *preselected);

#ifdef __cplusplus
}
#endif

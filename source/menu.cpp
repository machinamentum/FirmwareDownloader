#include "menu.h"

typedef struct {
  int ld;
  int index;
  std::string titleid;
  std::string titlekey;
  std::string name;
  std::string region;
  std::string code;
} game_item;

void titles_multkey_draw(const char *title, const char* footer, int back, std::vector<game_item> *options, void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    
    int count = options->size();
    int current = 0;
    int previous_index = 1;
    int menu_offset = 0;
    int previous_menu_offset = 1;
    int menu_pos_y;
    int menu_end_y = currentMenu.menuConsole.consoleHeight -1; 
    int current_pos_y = 0;



    // Don't allow the menu to draw beyond the edge of the screen, just truncate if so
/*    for (int i = 1; i < count && i < (currentMenu.menuConsole.consoleHeight - 2); i++) {
        pos_y_text[i] = current_pos_y;
        menu_draw_string(options[i], 1, current_pos_y++, CONSOLE_WHITE);
    }
*/
    while (true) {
        if(previous_index != current) {
            int results_per_page = menu_end_y - menu_pos_y;
            //results_per_page = 10;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            char prev_entry[50];
            char entry[50];
            sprintf(prev_entry, "%-30s (%s) %s",
                (*options)[previous_index].name.c_str(),
                (*options)[previous_index].region.c_str(),
                (*options)[previous_index].code.c_str());
            sprintf(entry, "%-30s (%s) %s",
                (*options)[current].name.c_str(),
                (*options)[current].region.c_str(),
                (*options)[current].code.c_str());
            if(menu_offset == previous_menu_offset) {
                menu_draw_string(prev_entry, 1, menu_pos_y + (previous_index-menu_offset), CONSOLE_WHITE);
                menu_draw_string(entry, 1, menu_pos_y + (current-menu_offset), CONSOLE_REVERSE);
            } else {
                consoleClear();
                current_pos_y=0;
                // Draw the header
                menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);
                menu_pos_y = current_pos_y;
                for (int i = 0; menu_offset + i < count && i < results_per_page; i++) {
                    sprintf(entry, "%-30s (%s) %s",
                        (*options)[menu_offset + i].name.c_str(),
                        (*options)[menu_offset + i].region.c_str(),
                        (*options)[menu_offset + i].code.c_str());
                    if(i+menu_offset == current) {
                        menu_draw_string(entry, 1, current_pos_y, CONSOLE_REVERSE);
                    } else {
                        menu_draw_string(entry, 1, current_pos_y, CONSOLE_WHITE);
                    }
                    current_pos_y++;
                    previous_menu_offset = menu_offset;
                }
                if (footer != NULL)
                {
                    // Draw the footer if one is provided
                    current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                    menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
                }
            }
            previous_index = current;
        }
        u32 key = wait_key();

        // If key is 0, it means aptMainLoop() returned false
        if (!key) {
            break;
        }
        
        if (key & KEY_UP) {
            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= count) current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}

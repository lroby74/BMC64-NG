









#ifndef RASPI_MENU_TEXT_LAYOUT_H
#define RASPI_MENU_TEXT_LAYOUT_H





#define MENU_TEXT_LAYOUT_US 0
#define MENU_TEXT_LAYOUT_IT 1
#define MENU_TEXT_LAYOUT_UK 2
#define MENU_TEXT_LAYOUT_DE 3




char menu_text_layout_key_to_char(long key, int shifted,
                                  int keyboard_mapping, int keyboard_layout);

#endif

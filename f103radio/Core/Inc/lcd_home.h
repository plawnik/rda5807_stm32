#ifndef LCD_HOME_H
#define LCD_HOME_H

#include "lcd_layout.h"
#include "lcd_ui.h"

typedef enum { LCD_ALIGN_LEFT, LCD_ALIGN_CENTER, LCD_ALIGN_RIGHT } lcd_align_t;

void lcd_draw_text(pcd8544_t *lcd, const lcd_font_t *font, int16_t x,
                   int16_t y, uint8_t width, const char *text,
                   lcd_align_t align, bool scroll, uint32_t elapsed_ms);
void lcd_home_render(lcd_ui_t *ui, const radio_app_t *app, uint32_t now_ms);
uint8_t lcd_home_signal_level(uint8_t rssi, bool stereo, bool rds);

#endif /* LCD_HOME_H */

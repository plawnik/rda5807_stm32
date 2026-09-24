#ifndef LCD_UI_H
#define LCD_UI_H

#include "input.h"
#include "pcd8544.h"
#include "radio_app.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  LCD_UI_HOME = 0,
  LCD_UI_MENU
} lcd_ui_screen_t;

typedef struct {
  pcd8544_t *lcd;
  lcd_ui_screen_t screen;
  radio_menu_item_t selected;
  uint32_t last_render_ms;
  uint32_t rendered_revision;
  uint8_t configured_contrast;
  uint8_t configured_bias;
  bool editing;
  bool configured_inverted;
  bool configured_backlight;
} lcd_ui_t;

void lcd_ui_init(lcd_ui_t *ui, pcd8544_t *lcd,
                 const radio_settings_t *settings);
void lcd_ui_handle_input(lcd_ui_t *ui, radio_app_t *app,
                         input_event_t event, uint32_t now_ms);
void lcd_ui_render(lcd_ui_t *ui, radio_app_t *app, uint32_t now_ms);

#endif /* LCD_UI_H */

#ifndef LCD_UI_H
#define LCD_UI_H

#include "input.h"
#include "pcd8544.h"
#include "radio_app.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  LCD_UI_HOME = 0,
  LCD_UI_CATEGORIES,
  LCD_UI_ITEMS,
  LCD_UI_EDIT,
  LCD_UI_STATION_LIST,
  LCD_UI_STATION_ACTIONS,
  LCD_UI_STATION_FREQUENCY,
  LCD_UI_STATION_NAME,
  LCD_UI_STATION_DELETE,
  LCD_UI_DEFAULTS,
  LCD_UI_ACTION,
  LCD_UI_CLOCK
} lcd_ui_screen_t;

typedef struct {
  pcd8544_t *lcd;
  lcd_ui_screen_t screen;
  radio_menu_item_t edited_item;
  uint32_t last_render_ms;
  uint32_t rendered_revision;
  uint32_t station_frequency_khz;
  uint32_t logo_slot;
  uint32_t home_text_key;
  uint32_t home_text_started_ms;
  uint32_t menu_scroll_started_ms;
  uint8_t configured_contrast;
  uint8_t configured_bias;
  uint8_t category;
  uint8_t selected;
  uint8_t station_index;
  uint8_t name_cursor;
  uint8_t home_action;
  uint8_t logo_variant;
  uint8_t clock_hour;
  uint8_t clock_minute;
  uint8_t clock_field;
  char station_name[RADIO_STATION_NAME_LENGTH + 1U];
  bool name_editing;
  bool clock_error;
  bool station_is_new;
  bool delete_confirmed;
  bool configured_inverted;
  bool configured_backlight;
} lcd_ui_t;

void lcd_ui_init(lcd_ui_t *ui, pcd8544_t *lcd,
                 const radio_settings_t *settings);
void lcd_ui_reset(lcd_ui_t *ui);
void lcd_ui_handle_input(lcd_ui_t *ui, radio_app_t *app,
                         input_event_t event, uint32_t now_ms);
void lcd_ui_render(lcd_ui_t *ui, radio_app_t *app, uint32_t now_ms);

#endif /* LCD_UI_H */

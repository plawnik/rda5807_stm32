#include "lcd_ui.h"

#include <stdio.h>
#include <string.h>

#define LCD_RENDER_INTERVAL_MS 100U
#define LCD_SCROLL_INTERVAL_MS 300U
#define LCD_VISIBLE_ROWS 5U

typedef struct {
  const char *label;
  const radio_menu_item_t *items;
  uint8_t item_count;
} lcd_category_t;

enum {
  LCD_CATEGORY_RADIO = 0,
  LCD_CATEGORY_AUDIO,
  LCD_CATEGORY_RECEPTION,
  LCD_CATEGORY_RDS,
  LCD_CATEGORY_CONTROLS,
  LCD_CATEGORY_DISPLAY,
  LCD_CATEGORY_STATIONS,
  LCD_CATEGORY_SYSTEM,
  LCD_CATEGORY_EXIT,
  LCD_CATEGORY_COUNT
};

static const radio_menu_item_t radio_items[] = {
    RADIO_MENU_FREQUENCY, RADIO_MENU_EXTENDED_RANGE,
    RADIO_MENU_SEEK_UP, RADIO_MENU_SEEK_DOWN};
static const radio_menu_item_t audio_items[] = {
    RADIO_MENU_VOLUME, RADIO_MENU_MUTE, RADIO_MENU_AUDIO_MODE,
    RADIO_MENU_BASS, RADIO_MENU_DEEMPHASIS};
static const radio_menu_item_t reception_items[] = {
    RADIO_MENU_SEEK_LIMIT, RADIO_MENU_SEEK_ALGORITHM,
    RADIO_MENU_SEEK_THRESHOLD, RADIO_MENU_RSSI_AVERAGE,
    RADIO_MENU_OLD_SEEK_THRESHOLD, RADIO_MENU_SOFTMUTE,
    RADIO_MENU_SOFTBLEND, RADIO_MENU_SOFTBLEND_THRESHOLD,
    RADIO_MENU_AFC, RADIO_MENU_NEW_METHOD, RADIO_MENU_LNA_INPUT,
    RADIO_MENU_LNA_CURRENT};
static const radio_menu_item_t rds_items[] = {
    RADIO_MENU_RDS, RADIO_MENU_RBDS};
static const radio_menu_item_t control_items[] = {
    RADIO_MENU_ENCODER_ACTION, RADIO_MENU_BUTTON_ACTION};
static const radio_menu_item_t display_items[] = {
    RADIO_MENU_LCD_CONTRAST, RADIO_MENU_LCD_INVERT,
    RADIO_MENU_LCD_BACKLIGHT};
static const radio_menu_item_t system_items[] = {RADIO_MENU_DEFAULTS};

static const lcd_category_t categories[LCD_CATEGORY_COUNT] = {
    {"Radio", radio_items, sizeof(radio_items) / sizeof(radio_items[0])},
    {"Dzwiek", audio_items, sizeof(audio_items) / sizeof(audio_items[0])},
    {"Odbior", reception_items,
     sizeof(reception_items) / sizeof(reception_items[0])},
    {"RDS", rds_items, sizeof(rds_items) / sizeof(rds_items[0])},
    {"Sterowanie", control_items,
     sizeof(control_items) / sizeof(control_items[0])},
    {"Ekran", display_items,
     sizeof(display_items) / sizeof(display_items[0])},
    {"Stacje", NULL, 0U},
    {"System", system_items, sizeof(system_items) / sizeof(system_items[0])},
    {"Wyjscie", NULL, 0U}};

static const uint8_t compact_digits[10][5] = {
    {0x07U, 0x05U, 0x05U, 0x05U, 0x07U},
    {0x02U, 0x06U, 0x02U, 0x02U, 0x07U},
    {0x07U, 0x01U, 0x07U, 0x04U, 0x07U},
    {0x07U, 0x01U, 0x07U, 0x01U, 0x07U},
    {0x05U, 0x05U, 0x07U, 0x01U, 0x01U},
    {0x07U, 0x04U, 0x07U, 0x01U, 0x07U},
    {0x07U, 0x04U, 0x07U, 0x05U, 0x07U},
    {0x07U, 0x01U, 0x01U, 0x01U, 0x01U},
    {0x07U, 0x05U, 0x07U, 0x05U, 0x07U},
    {0x07U, 0x05U, 0x07U, 0x01U, 0x07U}};

static uint8_t wrap_position(int32_t value, uint8_t count) {
  if (count == 0U) return 0U;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return (uint8_t)value;
}

static int16_t navigation_delta(input_event_t event) {
  if (event.rotation != 0) return event.rotation;
  if (event.left && !event.right) return -1;
  if (event.right && !event.left) return 1;
  return 0;
}

static bool accepted(input_event_t event) {
  return event.click || event.ok;
}

static size_t visible_length(const char *text, size_t maximum) {
  size_t length = strnlen(text, maximum);
  while (length > 0U && text[length - 1U] == ' ') --length;
  return length;
}

static void scrolling_text(char *output, size_t output_size,
                           const char *input, size_t input_limit,
                           size_t width, uint32_t now_ms) {
  const size_t length = visible_length(input, input_limit);
  size_t count;
  if (output_size == 0U) return;
  if (length <= width) {
    const size_t copy = length < output_size - 1U ? length : output_size - 1U;
    memcpy(output, input, copy);
    output[copy] = '\0';
    return;
  }

  count = width < output_size - 1U ? width : output_size - 1U;
  for (size_t index = 0U; index < count; ++index) {
    const size_t cycle = length + 3U;
    const size_t position =
        ((now_ms / LCD_SCROLL_INTERVAL_MS) + index) % cycle;
    output[index] = position < length ? input[position] : ' ';
  }
  output[count] = '\0';
}

static uint8_t list_first_row(uint8_t selected, uint8_t count) {
  uint8_t first = selected > 2U ? (uint8_t)(selected - 2U) : 0U;
  if (count > LCD_VISIBLE_ROWS &&
      first > (uint8_t)(count - LCD_VISIBLE_ROWS)) {
    first = (uint8_t)(count - LCD_VISIBLE_ROWS);
  }
  return first;
}

static void draw_header(pcd8544_t *lcd, const char *title) {
  char shown[15];
  snprintf(shown, sizeof(shown), "%-14.14s", title);
  pcd8544_text(lcd, 0, 0, shown, 1U, true);
  pcd8544_invert_rect(lcd, 0, 0, PCD8544_WIDTH, 8);
}

static void draw_list_label(pcd8544_t *lcd, int16_t y, const char *label,
                            bool selected, uint32_t now_ms) {
  char shown[15];
  scrolling_text(shown, sizeof(shown), label, 63U, 13U,
                 selected ? now_ms : 0U);
  pcd8544_text(lcd, 6, y, shown, 1U, true);
  if (selected) {
    pcd8544_text(lcd, 0, y, ">", 1U, true);
    pcd8544_invert_rect(lcd, 0, y, PCD8544_WIDTH, 8);
  }
}

static void frequency_digits(uint32_t frequency_khz, char digits[7]) {
  if (frequency_khz > 999999U) frequency_khz = 999999U;
  for (int8_t index = 5; index >= 0; --index) {
    digits[index] = (char)('0' + (frequency_khz % 10U));
    frequency_khz /= 10U;
  }
  digits[6] = '\0';
}

static void draw_signal_icon(pcd8544_t *lcd, int16_t x, int16_t y,
                             uint8_t rssi) {
  const uint8_t bars = (uint8_t)((rssi + 25U) / 26U);
  for (uint8_t index = 0U; index < 5U; ++index) {
    const int16_t height = (int16_t)(2 + index);
    pcd8544_rect(lcd, (int16_t)(x + index * 3),
                 (int16_t)(y + 7 - height), 2, height, true);
    if (index < bars) {
      pcd8544_fill_rect(lcd, (int16_t)(x + index * 3),
                        (int16_t)(y + 7 - height), 2, height, true);
    }
  }
}

static void draw_volume_icon(pcd8544_t *lcd, int16_t x, int16_t y,
                             uint8_t volume, bool muted) {
  pcd8544_fill_rect(lcd, x, (int16_t)(y + 2), 2, 4, true);
  pcd8544_line(lcd, (int16_t)(x + 2), (int16_t)(y + 2),
               (int16_t)(x + 5), y, true);
  pcd8544_line(lcd, (int16_t)(x + 2), (int16_t)(y + 5),
               (int16_t)(x + 5), (int16_t)(y + 7), true);
  pcd8544_line(lcd, (int16_t)(x + 5), y,
               (int16_t)(x + 5), (int16_t)(y + 7), true);
  if (muted) {
    pcd8544_line(lcd, (int16_t)(x + 8), (int16_t)(y + 1),
                 (int16_t)(x + 13), (int16_t)(y + 6), true);
    pcd8544_line(lcd, (int16_t)(x + 13), (int16_t)(y + 1),
                 (int16_t)(x + 8), (int16_t)(y + 6), true);
  } else {
    const uint8_t waves = (uint8_t)((volume + 4U) / 5U);
    for (uint8_t index = 0U; index < waves; ++index) {
      pcd8544_line(lcd, (int16_t)(x + 8 + index * 2),
                   (int16_t)(y + 2 - index),
                   (int16_t)(x + 8 + index * 2),
                   (int16_t)(y + 5 + index), true);
    }
  }
}

static void draw_compact_frequency(pcd8544_t *lcd, uint32_t frequency_khz) {
  char digits[7];
  int16_t x = 10;
  const int16_t y = 10;
  frequency_digits(frequency_khz, digits);
  for (uint8_t index = 0U; index < 6U; ++index) {
    const uint8_t glyph = (uint8_t)(digits[index] - '0');
    const bool leading_blank = index == 0U && digits[index] == '0';
    if (!leading_blank) {
      for (uint8_t row = 0U; row < 5U; ++row) {
        for (uint8_t column = 0U; column < 3U; ++column) {
          if ((compact_digits[glyph][row] &
               (1U << (2U - column))) != 0U) {
            pcd8544_fill_rect(lcd, (int16_t)(x + column * 2),
                              (int16_t)(y + row * 2), 2, 2, true);
          }
        }
      }
    }
    x = (int16_t)(x + 7);
    if (index == 2U) {
      pcd8544_fill_rect(lcd, x, (int16_t)(y + 8), 2, 2, true);
      x = (int16_t)(x + 3);
    }
  }
  pcd8544_text(lcd, (int16_t)(x + 1), (int16_t)(y + 2), "MHz", 1U, true);
}

static void render_home(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char line[20];
  char source[66] = {0};
  const rds_decoder_t *rds = &app->rds;
  const rda5807_status_t *status = &app->radio.status;
  size_t title_length;
  int16_t x;

  if (!app->radio_available) {
    pcd8544_text(ui->lcd, 12, 7, "BRAK RADIA", 1U, true);
    pcd8544_text(ui->lcd, 9, 22, "Sprawdz I2C", 1U, true);
    pcd8544_text(ui->lcd, 9, 36, "PB10 / PB11", 1U, true);
    return;
  }

  if (rds->ps_valid) memcpy(source, rds->program_service, 8U);
  else strcpy(source, "FM RADIO");
  title_length = visible_length(source, 8U);
  x = (int16_t)((PCD8544_WIDTH - title_length * 6U) / 2U);
  pcd8544_text(ui->lcd, x, 0, source, 1U, true);
  if (status->rds_synchronized) pcd8544_text(ui->lcd, 0, 0, "R", 1U, true);
  if (app->radio.operation != RDA5807_OPERATION_IDLE) {
    pcd8544_text(ui->lcd, 78, 0, ">", 1U, true);
  }

  draw_compact_frequency(ui->lcd, app->settings.frequency_khz);
  draw_volume_icon(ui->lcd, 1, 26, app->settings.volume,
                   app->settings.muted);
  draw_signal_icon(ui->lcd, 26, 26, radio_app_display_rssi(app));
  snprintf(line, sizeof(line), "%c %03u",
           status->stereo && !app->settings.force_mono ? 'S' : 'M',
           radio_app_display_rssi(app));
  pcd8544_text(ui->lcd, 43, 26, line, 1U, true);

  if (rds->radio_text_valid || rds->rt_segments != 0U) {
    scrolling_text(line, sizeof(line), rds->radio_text, 64U, 14U, now_ms);
  } else {
    snprintf(source, sizeof(source), "%s PI:%04X",
             rds_program_type_name(rds->program_type), rds->program_id);
    scrolling_text(line, sizeof(line), source, sizeof(source), 14U, now_ms);
  }
  pcd8544_text(ui->lcd, 0, 40, line, 1U, true);
}

static void render_categories(lcd_ui_t *ui, uint32_t now_ms) {
  const uint8_t first = list_first_row(ui->selected, LCD_CATEGORY_COUNT);
  draw_header(ui->lcd, "MENU");
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    if (index >= LCD_CATEGORY_COUNT) break;
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U),
                    categories[index].label, index == ui->selected, now_ms);
  }
}

static void render_items(lcd_ui_t *ui, const radio_app_t *app,
                         uint32_t now_ms) {
  const lcd_category_t *category = &categories[ui->category];
  const uint8_t count = (uint8_t)(category->item_count + 1U);
  const uint8_t first = list_first_row(ui->selected, count);
  draw_header(ui->lcd, category->label);
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    char label[64];
    if (index >= count) break;
    if (index == 0U) {
      strcpy(label, "Wstecz");
    } else {
      const radio_menu_item_t item = category->items[index - 1U];
      if (index == ui->selected) {
        char value[28];
        radio_app_menu_value(app, item, value, sizeof(value));
        snprintf(label, sizeof(label), "%s: %s",
                 radio_app_menu_label(item), value);
      } else {
        snprintf(label, sizeof(label), "%s", radio_app_menu_label(item));
      }
    }
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U), label,
                    index == ui->selected, now_ms);
  }
}

static void render_edit(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char label[24];
  char value[28];
  draw_header(ui->lcd, "EDYCJA");
  scrolling_text(label, sizeof(label), radio_app_menu_label(ui->edited_item),
                 40U, 14U, now_ms);
  pcd8544_text(ui->lcd, 0, 10, label, 1U, true);
  radio_app_menu_value(app, ui->edited_item, value, sizeof(value));
  scrolling_text(label, sizeof(label), value, sizeof(value), 14U, now_ms);
  pcd8544_text(ui->lcd, 0, 24, label, 1U, true);
  pcd8544_invert_rect(ui->lcd, 0, 22, PCD8544_WIDTH, 11);
  pcd8544_text(ui->lcd, 0, 40, "OBROT zmiana OK", 1U, true);
}

static void station_label(const radio_station_t *station, char *buffer,
                          size_t size) {
  char name[RADIO_STATION_NAME_LENGTH + 1U];
  memcpy(name, station->name, sizeof(name));
  name[RADIO_STATION_NAME_LENGTH] = '\0';
  snprintf(buffer, size, "%s %lu.%03lu", name,
           (unsigned long)(station->frequency_khz / 1000U),
           (unsigned long)(station->frequency_khz % 1000U));
}

static void render_station_list(lcd_ui_t *ui, const radio_app_t *app,
                                uint32_t now_ms) {
  const uint8_t count = (uint8_t)(app->settings.station_count + 2U);
  const uint8_t first = list_first_row(ui->selected, count);
  draw_header(ui->lcd, "STACJE");
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    char label[40];
    if (index >= count) break;
    if (index == 0U) strcpy(label, "Wstecz");
    else if (index == 1U) {
      snprintf(label, sizeof(label), "%s",
               app->settings.station_count < RADIO_MAX_STATIONS
                   ? "Dodaj stacje" : "Lista pelna");
    } else {
      station_label(&app->settings.stations[index - 2U], label,
                    sizeof(label));
    }
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U), label,
                    index == ui->selected, now_ms);
  }
}

static void render_station_actions(lcd_ui_t *ui, const radio_app_t *app,
                                   uint32_t now_ms) {
  static const char *const labels[] = {
      "Wstecz", "Wybierz", "Czestotliwosc", "Nazwa", "Usun"};
  char title[15];
  const uint8_t first = list_first_row(ui->selected, 5U);
  if (ui->station_index < app->settings.station_count) {
    snprintf(title, sizeof(title), "%.8s",
             app->settings.stations[ui->station_index].name);
  } else {
    strcpy(title, "STACJA");
  }
  draw_header(ui->lcd, title);
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    if (index >= 5U) break;
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U), labels[index],
                    index == ui->selected, now_ms);
  }
}

static void render_station_frequency(lcd_ui_t *ui) {
  char frequency[16];
  draw_header(ui->lcd, ui->station_is_new ? "NOWA STACJA" : "CZESTOTLIWOSC");
  snprintf(frequency, sizeof(frequency), "%03lu.%03lu MHz",
           (unsigned long)(ui->station_frequency_khz / 1000U),
           (unsigned long)(ui->station_frequency_khz % 1000U));
  pcd8544_text(ui->lcd, 6, 15, frequency, 1U, true);
  pcd8544_invert_rect(ui->lcd, 3, 12, 78, 13);
  pcd8544_text(ui->lcd, 0, 30, "L/P lub obrot", 1U, true);
  pcd8544_text(ui->lcd, 0, 40, "OK: dalej", 1U, true);
}

static void render_station_name(lcd_ui_t *ui) {
  draw_header(ui->lcd, "NAZWA STACJI");
  pcd8544_text(ui->lcd, 18, 15, ui->station_name, 1U, true);
  pcd8544_invert_rect(ui->lcd, (int16_t)(18 + ui->name_cursor * 6), 13, 6, 11);
  pcd8544_text(ui->lcd, 0, 29, "L/P: znak", 1U, true);
  pcd8544_text(ui->lcd, 0, 40,
               ui->name_cursor + 1U < RADIO_STATION_NAME_LENGTH
                   ? "OK: nastepny" : "OK: zapisz", 1U, true);
}

static void render_station_delete(lcd_ui_t *ui) {
  draw_header(ui->lcd, "USUN STACJE?");
  pcd8544_text(ui->lcd, 12, 16,
               ui->delete_confirmed ? "NIE    [TAK]" : "[NIE]    TAK",
               1U, true);
  pcd8544_text(ui->lcd, 0, 32, "L/P: wybor", 1U, true);
  pcd8544_text(ui->lcd, 0, 40, "OK: zatwierdz", 1U, true);
}

static void initialize_station_name(char name[RADIO_STATION_NAME_LENGTH + 1U],
                                    const char *source) {
  memset(name, ' ', RADIO_STATION_NAME_LENGTH);
  name[RADIO_STATION_NAME_LENGTH] = '\0';
  if (source == NULL) return;
  for (uint8_t index = 0U;
       index < RADIO_STATION_NAME_LENGTH && source[index] != '\0'; ++index) {
    name[index] = source[index];
  }
}

static void adjust_station_frequency(lcd_ui_t *ui, const radio_app_t *app,
                                     int16_t delta) {
  const int64_t maximum = app->settings.extended_tuning
                              ? RADIO_REGISTER_MAX_KHZ
                              : RADIO_SYNTH_MAX_KHZ;
  int64_t value = (int64_t)ui->station_frequency_khz +
                  (int64_t)delta * 50;
  if (value < RADIO_SYNTH_MIN_KHZ) value = RADIO_SYNTH_MIN_KHZ;
  if (value > maximum) value = maximum;
  ui->station_frequency_khz = (uint32_t)value;
}

static void adjust_station_character(lcd_ui_t *ui, int16_t delta) {
  static const char alphabet[] =
      " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._";
  const size_t count = sizeof(alphabet) - 1U;
  size_t position = 0U;
  while (position < count &&
         alphabet[position] != ui->station_name[ui->name_cursor]) {
    ++position;
  }
  if (position == count) position = 0U;
  while (delta > 0) {
    position = (position + 1U) % count;
    --delta;
  }
  while (delta < 0) {
    position = position == 0U ? count - 1U : position - 1U;
    ++delta;
  }
  ui->station_name[ui->name_cursor] = alphabet[position];
}

void lcd_ui_init(lcd_ui_t *ui, pcd8544_t *lcd,
                 const radio_settings_t *settings) {
  if (ui == NULL || lcd == NULL || settings == NULL) return;
  memset(ui, 0, sizeof(*ui));
  ui->lcd = lcd;
  ui->configured_contrast = settings->lcd_contrast;
  ui->configured_bias = settings->lcd_bias;
  ui->configured_inverted = settings->lcd_inverted;
  ui->configured_backlight = settings->lcd_backlight;
}

void lcd_ui_reset(lcd_ui_t *ui) {
  if (ui == NULL) return;
  ui->screen = LCD_UI_HOME;
  ui->category = 0U;
  ui->selected = 0U;
  ui->rendered_revision = 0U;
  ui->last_render_ms = 0U;
}

void lcd_ui_handle_input(lcd_ui_t *ui, radio_app_t *app,
                         input_event_t event, uint32_t now_ms) {
  int16_t delta;
  bool accept;
  if (ui == NULL || app == NULL) return;
  delta = navigation_delta(event);
  accept = accepted(event);

  switch (ui->screen) {
    case LCD_UI_HOME:
      if (event.rotation != 0) {
        radio_app_control_left_right(app, app->settings.encoder_action,
                                     event.rotation, now_ms);
      }
      if (event.left) {
        radio_app_control_left_right(app, app->settings.buttons_action,
                                     -1, now_ms);
      }
      if (event.right) {
        radio_app_control_left_right(app, app->settings.buttons_action,
                                     1, now_ms);
      }
      if (accept) {
        ui->screen = LCD_UI_CATEGORIES;
        ui->selected = 0U;
      }
      break;

    case LCD_UI_CATEGORIES:
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta,
                                     LCD_CATEGORY_COUNT);
      }
      if (accept) {
        ui->category = ui->selected;
        ui->selected = 0U;
        if (ui->category == LCD_CATEGORY_EXIT) ui->screen = LCD_UI_HOME;
        else if (ui->category == LCD_CATEGORY_STATIONS) {
          ui->screen = LCD_UI_STATION_LIST;
        } else {
          ui->screen = LCD_UI_ITEMS;
        }
      }
      break;

    case LCD_UI_ITEMS: {
      const lcd_category_t *category = &categories[ui->category];
      const uint8_t count = (uint8_t)(category->item_count + 1U);
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta, count);
      }
      if (accept) {
        if (ui->selected == 0U) {
          ui->screen = LCD_UI_CATEGORIES;
          ui->selected = ui->category;
        } else {
          const radio_menu_item_t item = category->items[ui->selected - 1U];
          if (radio_app_menu_is_action(item)) {
            radio_app_menu_activate(app, item, now_ms);
          } else {
            ui->edited_item = item;
            ui->screen = LCD_UI_EDIT;
          }
        }
      }
      break;
    }

    case LCD_UI_EDIT:
      if (delta != 0) {
        radio_app_menu_adjust(app, ui->edited_item, delta, now_ms);
      }
      if (accept) ui->screen = LCD_UI_ITEMS;
      break;

    case LCD_UI_STATION_LIST: {
      const uint8_t count = (uint8_t)(app->settings.station_count + 2U);
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta, count);
      }
      if (accept) {
        if (ui->selected == 0U) {
          ui->screen = LCD_UI_CATEGORIES;
          ui->selected = LCD_CATEGORY_STATIONS;
        } else if (ui->selected == 1U) {
          if (app->settings.station_count < RADIO_MAX_STATIONS) {
            ui->station_is_new = true;
            ui->station_frequency_khz = app->settings.frequency_khz;
            initialize_station_name(ui->station_name, "STACJA");
            ui->name_cursor = 0U;
            ui->screen = LCD_UI_STATION_FREQUENCY;
          }
        } else {
          ui->station_index = (uint8_t)(ui->selected - 2U);
          ui->station_is_new = false;
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_ACTIONS;
        }
      }
      break;
    }

    case LCD_UI_STATION_ACTIONS:
      if (ui->station_index >= app->settings.station_count) {
        ui->screen = LCD_UI_STATION_LIST;
        ui->selected = 0U;
        break;
      }
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta, 5U);
      }
      if (accept) {
        const radio_station_t *station =
            &app->settings.stations[ui->station_index];
        switch (ui->selected) {
          case 0U:
            ui->screen = LCD_UI_STATION_LIST;
            ui->selected = (uint8_t)(ui->station_index + 2U);
            break;
          case 1U:
            radio_app_station_tune(app, ui->station_index, now_ms);
            ui->screen = LCD_UI_HOME;
            break;
          case 2U:
            ui->station_frequency_khz = station->frequency_khz;
            ui->screen = LCD_UI_STATION_FREQUENCY;
            break;
          case 3U:
            initialize_station_name(ui->station_name, station->name);
            ui->name_cursor = 0U;
            ui->screen = LCD_UI_STATION_NAME;
            break;
          case 4U:
            ui->delete_confirmed = false;
            ui->screen = LCD_UI_STATION_DELETE;
            break;
          default:
            break;
        }
      }
      break;

    case LCD_UI_STATION_FREQUENCY:
      if (delta != 0) adjust_station_frequency(ui, app, delta);
      if (accept) {
        if (ui->station_is_new) {
          ui->name_cursor = 0U;
          ui->screen = LCD_UI_STATION_NAME;
        } else {
          radio_app_station_update(app, ui->station_index,
                                   ui->station_frequency_khz, NULL, now_ms);
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_ACTIONS;
        }
      }
      break;

    case LCD_UI_STATION_NAME:
      if (delta != 0) adjust_station_character(ui, delta);
      if (accept) {
        if (ui->name_cursor + 1U < RADIO_STATION_NAME_LENGTH) {
          ++ui->name_cursor;
        } else if (ui->station_is_new) {
          if (radio_app_station_add(app, ui->station_frequency_khz,
                                    ui->station_name, now_ms)) {
            ui->station_index = (uint8_t)(app->settings.station_count - 1U);
            ui->station_is_new = false;
            ui->selected = 0U;
            ui->screen = LCD_UI_STATION_ACTIONS;
          }
        } else {
          radio_app_station_update(app, ui->station_index,
                                   app->settings.stations[ui->station_index]
                                       .frequency_khz,
                                   ui->station_name, now_ms);
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_ACTIONS;
        }
      }
      break;

    case LCD_UI_STATION_DELETE:
      if (delta != 0) ui->delete_confirmed = !ui->delete_confirmed;
      if (accept) {
        if (ui->delete_confirmed) {
          radio_app_station_delete(app, ui->station_index, now_ms);
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_LIST;
        } else {
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_ACTIONS;
        }
      }
      break;

    default:
      lcd_ui_reset(ui);
      break;
  }
  ui->rendered_revision = 0U;
}

void lcd_ui_render(lcd_ui_t *ui, radio_app_t *app, uint32_t now_ms) {
  const radio_settings_t *settings;
  if (ui == NULL || app == NULL || ui->lcd == NULL) return;
  if ((uint32_t)(now_ms - ui->last_render_ms) < LCD_RENDER_INTERVAL_MS) return;
  ui->last_render_ms = now_ms;
  settings = &app->settings;

  if (ui->configured_contrast != settings->lcd_contrast ||
      ui->configured_bias != settings->lcd_bias ||
      ui->configured_inverted != settings->lcd_inverted ||
      ui->configured_backlight != settings->lcd_backlight) {
    pcd8544_configure(ui->lcd, settings->lcd_contrast, settings->lcd_bias,
                      settings->lcd_inverted, settings->lcd_backlight);
    ui->configured_contrast = settings->lcd_contrast;
    ui->configured_bias = settings->lcd_bias;
    ui->configured_inverted = settings->lcd_inverted;
    ui->configured_backlight = settings->lcd_backlight;
  }

  pcd8544_clear(ui->lcd);
  switch (ui->screen) {
    case LCD_UI_HOME: render_home(ui, app, now_ms); break;
    case LCD_UI_CATEGORIES: render_categories(ui, now_ms); break;
    case LCD_UI_ITEMS: render_items(ui, app, now_ms); break;
    case LCD_UI_EDIT: render_edit(ui, app, now_ms); break;
    case LCD_UI_STATION_LIST: render_station_list(ui, app, now_ms); break;
    case LCD_UI_STATION_ACTIONS:
      render_station_actions(ui, app, now_ms); break;
    case LCD_UI_STATION_FREQUENCY: render_station_frequency(ui); break;
    case LCD_UI_STATION_NAME: render_station_name(ui); break;
    case LCD_UI_STATION_DELETE: render_station_delete(ui); break;
    default: break;
  }
  (void)pcd8544_update(ui->lcd);
  ui->rendered_revision = app->revision;
}

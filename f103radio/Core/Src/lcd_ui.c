#include "lcd_ui.h"

#include "radio_clock.h"
#include "lcd_home.h"
#include "lcd_layout.h"

#include <stdio.h>
#include <string.h>

#define LCD_RENDER_INTERVAL_MS 100U
#define LCD_VISIBLE_ROWS lcd_visible_rows
#define LCD_CATEGORY_EXIT lcd_category_count
#define LCD_CATEGORY_COUNT (lcd_category_count + 1U)
#define categories lcd_categories

static uint8_t station_category(void) {
  for (uint8_t c = 0U; c < lcd_category_count; ++c)
    for (uint8_t e = 0U; e < categories[c].entry_count; ++e)
      if (categories[c].entries[e].item == RADIO_MENU_STATION_LIST) return c;
  return 0U;
}

static uint8_t wrap_position(int32_t value, uint8_t count) {
  if (count == 0U) return 0U;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return (uint8_t)value;
}

static int16_t navigation_delta(input_event_t event) {
  int32_t delta = event.rotation;
  if (event.left) --delta;
  if (event.right) ++delta;
  if (delta > INT16_MAX) return INT16_MAX;
  if (delta < INT16_MIN) return INT16_MIN;
  return (int16_t)delta;
}

static bool accepted(input_event_t event) {
  return event.click || event.ok;
}

static uint8_t list_first_row(uint8_t selected, uint8_t count) {
  uint8_t first = selected > 2U ? (uint8_t)(selected - 2U) : 0U;
  const uint8_t maximum = count > LCD_VISIBLE_ROWS ? (uint8_t)(count - LCD_VISIBLE_ROWS) : 0U;
  if (first > maximum) first = maximum;
  return first;
}

static void draw_header(pcd8544_t *lcd, const char *title) {
  lcd_draw_text(lcd, &lcd_menu_font, 0, 0, 84, title,
                 LCD_ALIGN_CENTER, false, 0U);
  pcd8544_invert_rect(lcd, 0, 0, PCD8544_WIDTH, 8);
}

static void draw_list_label(pcd8544_t *lcd, int16_t y, const char *label,
                            bool selected, uint32_t elapsed_ms) {
  lcd_draw_text(lcd, &lcd_menu_font, 6, y, 78, label, LCD_ALIGN_LEFT,
                 selected, elapsed_ms);
  if (selected) {
    lcd_draw_text(lcd, &lcd_menu_font, 0, y, 6, ">", LCD_ALIGN_LEFT, false, 0U);
    pcd8544_invert_rect(lcd, 0, y, PCD8544_WIDTH, 8);
  }
}

static int32_t entry_value(const radio_app_t *app, radio_menu_item_t item) {
  const radio_settings_t *s = &app->settings;
  switch (item) {
    case RADIO_MENU_FREQUENCY: return (int32_t)s->frequency_khz;
    case RADIO_MENU_EXTENDED_RANGE: return s->extended_tuning;
    case RADIO_MENU_VOLUME: return s->volume;
    case RADIO_MENU_MUTE: return s->muted;
    case RADIO_MENU_AUDIO_MODE: return s->force_mono;
    case RADIO_MENU_BASS: return s->bass_boost;
    case RADIO_MENU_SEEK_THRESHOLD: return s->seek_threshold;
    case RADIO_MENU_RSSI_AVERAGE: return s->rssi_average_ms;
    case RADIO_MENU_AFC: return s->afc_enabled;
    case RADIO_MENU_RDS: return s->rds_enabled;
    case RADIO_MENU_RBDS: return s->rbds_enabled;
    case RADIO_MENU_ENCODER_ACTION: return s->encoder_action;
    case RADIO_MENU_BUTTON_ACTION: return s->buttons_action;
    case RADIO_MENU_LCD_CONTRAST: return s->lcd_contrast;
    case RADIO_MENU_LCD_INVERT: return s->lcd_inverted;
    case RADIO_MENU_LCD_BACKLIGHT: return s->lcd_backlight;
    case RADIO_MENU_CLOCK_RDS: return radio_clock_rds_sync_enabled();
    default: return 0;
  }
}

static void entry_value_label(const radio_app_t *app,
                               const lcd_menu_entry_t *entry,
                               char *text, size_t size) {
  if (entry->type == LCD_ENTRY_INFO) {
    snprintf(text, size, "%s", entry->message);
  } else if (entry->value_count != 0U) {
    uint8_t index = 0U;
    const int32_t value = entry_value(app, entry->item);
    while (index + 1U < entry->value_count && entry->choice_values[index] < value) ++index;
    snprintf(text, size, "%s", entry->values[index]);
  } else if (entry->item == RADIO_MENU_CLOCK_RDS) {
    snprintf(text, size, "%s", radio_clock_rds_sync_enabled() ? "WL." : "WYL.");
  } else {
    radio_app_menu_value(app, entry->item, text, size);
  }
}

static void adjust_entry(lcd_ui_t *ui, radio_app_t *app, int16_t delta,
                          uint32_t now_ms) {
  const lcd_menu_entry_t *entry = lcd_layout_entry(ui->edited_item);
  const int32_t current = entry_value(app, ui->edited_item);
  int32_t target;
  if (entry == NULL || entry->type == LCD_ENTRY_INFO) return;
  if (entry->item == RADIO_MENU_CLOCK_RDS) {
    radio_clock_set_rds_sync(!radio_clock_rds_sync_enabled());
    return;
  }
  if (entry->type == LCD_ENTRY_TOGGLE || entry->type == LCD_ENTRY_CHOICE ||
      entry->type == LCD_ENTRY_TEXT_CYCLE) {
    uint8_t index = 0U;
    while (index + 1U < entry->value_count && entry->choice_values[index] < current) ++index;
    index = wrap_position((int32_t)index + delta, entry->value_count);
    target = entry->choice_values[index];
  } else {
    int64_t value = (int64_t)current + (int64_t)delta * entry->step;
    const int32_t maximum = entry->item == RADIO_MENU_FREQUENCY && app->settings.extended_tuning
                              ? (int32_t)RADIO_REGISTER_MAX_KHZ : entry->maximum;
    if (value < entry->minimum) value = entry->minimum;
    if (value > maximum) value = maximum;
    target = (int32_t)value;
  }
  if (target == current) return;
  if (entry->item == RADIO_MENU_FREQUENCY) {
    radio_app_set_frequency_auto(app, target, 50U, now_ms);
  } else if (entry->item == RADIO_MENU_RSSI_AVERAGE) {
    radio_app_menu_adjust(app, entry->item,
                          (int16_t)((target - current) / (int32_t)RADIO_RSSI_AVERAGE_STEP_MS), now_ms);
  } else {
    radio_app_menu_adjust(app, entry->item, (int16_t)(target - current), now_ms);
  }
}

static void render_categories(lcd_ui_t *ui, uint32_t now_ms) {
  const uint8_t first = list_first_row(ui->selected, LCD_CATEGORY_COUNT);
  draw_header(ui->lcd, lcd_menu_title);
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    if (index >= LCD_CATEGORY_COUNT) break;
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U),
                    index == LCD_CATEGORY_EXIT ? "Wyjscie" : categories[index].label,
                    index == ui->selected, now_ms - ui->menu_scroll_started_ms);
  }
}

static void render_items(lcd_ui_t *ui, const radio_app_t *app,
                         uint32_t now_ms) {
  const lcd_category_t *category = &categories[ui->category];
  const uint8_t count = (uint8_t)(category->entry_count + 1U);
  const uint8_t first = list_first_row(ui->selected, count);
  draw_header(ui->lcd, category->label);
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    char label[64];
    if (index >= count) break;
    if (index == 0U) {
      strcpy(label, "Wstecz");
    } else {
      const lcd_menu_entry_t *entry = &category->entries[index - 1U];
      snprintf(label, sizeof(label), "%s", entry->label);
    }
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U), label,
                    index == ui->selected, now_ms - ui->menu_scroll_started_ms);
  }
}

static void render_edit(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  const lcd_menu_entry_t *entry = lcd_layout_entry(ui->edited_item);
  const uint32_t elapsed = now_ms - ui->menu_scroll_started_ms;
  char value[48];
  if (entry == NULL) return;
  lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 1, 84, entry->label,
                 LCD_ALIGN_CENTER, true, elapsed);
  pcd8544_invert_rect(ui->lcd, 0, 0, 84, 9);
  entry_value_label(app, entry, value, sizeof(value));
  if (entry->type == LCD_ENTRY_BAR) {
    const int32_t range = entry->maximum - entry->minimum;
    const int32_t value_now = entry_value(app, entry->item) - entry->minimum;
    const uint8_t active = range > 0 ? (uint8_t)((value_now * entry->segments + range / 2) / range) : 0U;
    pcd8544_rect(ui->lcd, 6, 21, 72, 10, true);
    /* Distribute segment boundaries over the full inner width. */
    for (uint8_t i = 0U; i < active && i < entry->segments; ++i) {
      const int16_t left = (int16_t)(8 + (uint16_t)i * 68U / entry->segments);
      const int16_t right = (int16_t)(8 + (uint16_t)(i + 1U) * 68U / entry->segments - 1);
      pcd8544_fill_rect(ui->lcd, left, 23, right - left, 6, true);
    }
    lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 36, 84, value, LCD_ALIGN_CENTER, true, elapsed);
  } else {
    lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 21, 84, value, LCD_ALIGN_CENTER, true, elapsed);
    lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 40, 84,
                   entry->type == LCD_ENTRY_INFO ? "OK: WSTECZ" : "< ZMIANA > OK",
                   LCD_ALIGN_CENTER, false, 0U);
  }
}

static void render_defaults(lcd_ui_t *ui, uint32_t now_ms) {
  const lcd_menu_entry_t *entry = lcd_layout_entry(RADIO_MENU_DEFAULTS);
  draw_header(ui->lcd, "RESET USTAWIEN");
  lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 12, 84, entry->message, LCD_ALIGN_CENTER,
                 true, now_ms - ui->menu_scroll_started_ms);
  pcd8544_text(ui->lcd, 6, 28, ui->delete_confirmed ? "NIE   [TAK]" : "[NIE]   TAK", 1U, true);
  pcd8544_text(ui->lcd, 0, 40, "OK: zatwierdz", 1U, true);
}

static void render_action(lcd_ui_t *ui, const radio_app_t *app, uint32_t now_ms) {
  const lcd_menu_entry_t *entry = lcd_layout_entry(ui->edited_item);
  const char *message = entry->message;
  char value[28];
  draw_header(ui->lcd, "WYSZUKIWANIE");
  if (app->radio.operation == RDA5807_OPERATION_IDLE) {
    if (!app->radio_available) message = "BRAK RADIA";
    else if (app->radio.status.seek_failed) message = "BRAK STACJI";
    else {
      radio_app_menu_value(app, RADIO_MENU_FREQUENCY, value, sizeof(value));
      message = value;
    }
  }
  lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 20, 84, message, LCD_ALIGN_CENTER,
                 true, now_ms - ui->menu_scroll_started_ms);
  pcd8544_text(ui->lcd, 12, 40, "OK: wstecz", 1U, true);
}

static void render_clock(lcd_ui_t *ui) {
  char text[8];
  draw_header(ui->lcd, "USTAW ZEGAR");
  snprintf(text, sizeof(text), "%02u:%02u", ui->clock_hour, ui->clock_minute);
  pcd8544_text(ui->lcd, 27, 16, text, 1U, true);
  pcd8544_invert_rect(ui->lcd, ui->clock_field == 0U ? 26 : 44, 14, 13, 11);
  pcd8544_text(ui->lcd, 6, 30, ui->clock_field == 0U ? "< GODZINA >" : "< MINUTA >", 1U, true);
  pcd8544_text(ui->lcd, 0, 40, ui->clock_error ? "BLAD RTC" :
               (ui->clock_field == 0U ? "OK: dalej" : "OK: zapisz"), 1U, true);
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
    char label[RADIO_STATION_NAME_LENGTH + 18U];
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
                    index == ui->selected, now_ms - ui->menu_scroll_started_ms);
  }
}

static void render_station_actions(lcd_ui_t *ui, const radio_app_t *app,
                                   uint32_t now_ms) {
  static const char *const labels[] = {
      "Wstecz", "Wybierz", "Czestotliwosc", "Nazwa", "Usun"};
  char title[RADIO_STATION_NAME_LENGTH + 1U];
  const uint8_t first = list_first_row(ui->selected, 5U);
  if (ui->station_index < app->settings.station_count) {
    snprintf(title, sizeof(title), "%s",
             app->settings.stations[ui->station_index].name);
  } else {
    strcpy(title, "STACJA");
  }
  lcd_draw_text(ui->lcd, &lcd_menu_font, 0, 0, 84, title, LCD_ALIGN_CENTER,
                 true, now_ms - ui->menu_scroll_started_ms);
  pcd8544_invert_rect(ui->lcd, 0, 0, 84, 8);
  for (uint8_t row = 0U; row < LCD_VISIBLE_ROWS; ++row) {
    const uint8_t index = (uint8_t)(first + row);
    if (index >= 5U) break;
    draw_list_label(ui->lcd, (int16_t)(8U + row * 8U), labels[index],
                    index == ui->selected, now_ms - ui->menu_scroll_started_ms);
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

static uint8_t name_positions(const lcd_ui_t *ui) {
  uint8_t length = RADIO_STATION_NAME_LENGTH;
  while (length > 0U && ui->station_name[length - 1U] == ' ') --length;
  return length < RADIO_STATION_NAME_LENGTH ? (uint8_t)(length + 1U) : length;
}

static void render_station_name(lcd_ui_t *ui) {
  char window[14];
  const uint8_t positions = name_positions(ui);
  const uint8_t cursor = ui->name_cursor < positions ? ui->name_cursor : 0U;
  uint8_t first = cursor > 6U ? (uint8_t)(cursor - 6U) : 0U;
  if (first > RADIO_STATION_NAME_LENGTH - 13U) first = RADIO_STATION_NAME_LENGTH - 13U;
  memcpy(window, &ui->station_name[first], 13U);
  window[13] = '\0';
  draw_header(ui->lcd, "NAZWA STACJI");
  pcd8544_text(ui->lcd, 3, 15, window, 1U, true);
  if (ui->name_cursor < positions)
    pcd8544_invert_rect(ui->lcd, (int16_t)(3 + (cursor - first) * 6), 13, 6, 11);
  pcd8544_text(ui->lcd, 0, 29,
               ui->name_cursor == positions ? "[ZAPISZ] ANULUJ" :
               (ui->name_cursor == positions + 1U ? "ZAPISZ [ANULUJ]" : "ZAPISZ  ANULUJ"), 1U, true);
  pcd8544_text(ui->lcd, 0, 40, ui->name_editing ? "L/P znak OK" :
               (ui->name_cursor < positions ? "OK: edytuj" : "OK: wybor"), 1U, true);
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
  ui->home_action = settings->encoder_action;
  ui->logo_slot = UINT32_MAX;
}

void lcd_ui_reset(lcd_ui_t *ui) {
  if (ui == NULL) return;
  ui->screen = LCD_UI_HOME;
  ui->category = 0U;
  ui->selected = 0U;
  ui->logo_slot = UINT32_MAX;
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
        ui->home_action = app->settings.encoder_action;
        radio_app_control_left_right(app, app->settings.encoder_action,
                                     event.rotation, now_ms);
      }
      if (event.left) {
        ui->home_action = app->settings.buttons_action;
        radio_app_control_left_right(app, app->settings.buttons_action,
                                     -1, now_ms);
      }
      if (event.right) {
        ui->home_action = app->settings.buttons_action;
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
        else {
          ui->screen = LCD_UI_ITEMS;
        }
      }
      break;

    case LCD_UI_ITEMS: {
      const lcd_category_t *category = &categories[ui->category];
      const uint8_t count = (uint8_t)(category->entry_count + 1U);
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta, count);
      }
      if (accept) {
        if (ui->selected == 0U) {
          ui->screen = LCD_UI_CATEGORIES;
          ui->selected = ui->category;
        } else {
          const lcd_menu_entry_t *entry = &category->entries[ui->selected - 1U];
          const radio_menu_item_t item = entry->item;
          if (item == RADIO_MENU_ADD_STATION) {
            if (app->settings.station_count < RADIO_MAX_STATIONS) {
              ui->station_is_new = true;
              ui->station_frequency_khz = app->settings.frequency_khz;
              initialize_station_name(ui->station_name, "STACJA");
              ui->name_cursor = 0U;
              ui->name_editing = false;
              ui->screen = LCD_UI_STATION_FREQUENCY;
            }
          } else if (item == RADIO_MENU_STATION_LIST) {
            ui->screen = LCD_UI_STATION_LIST;
            ui->selected = 0U;
          } else if (item == RADIO_MENU_DEFAULTS) {
            ui->delete_confirmed = false;
            ui->screen = LCD_UI_DEFAULTS;
          } else if (item == RADIO_MENU_CLOCK) {
            radio_clock_time_t time = {0};
            (void)radio_clock_read(&time);
            ui->clock_hour = time.hour;
            ui->clock_minute = time.minute;
            ui->clock_field = 0U;
            ui->clock_error = false;
            ui->screen = LCD_UI_CLOCK;
          } else if (radio_app_menu_is_action(item)) {
            ui->edited_item = item;
            radio_app_menu_activate(app, item, now_ms);
            ui->screen = LCD_UI_ACTION;
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
        adjust_entry(ui, app, delta, now_ms);
      }
      if (accept) ui->screen = LCD_UI_ITEMS;
      break;

    case LCD_UI_DEFAULTS:
      if (delta != 0) ui->delete_confirmed = !ui->delete_confirmed;
      if (accept) {
        if (ui->delete_confirmed) {
          radio_app_menu_activate(app, RADIO_MENU_DEFAULTS, now_ms);
          ui->home_action = app->settings.encoder_action;
        }
        ui->screen = LCD_UI_ITEMS;
      }
      break;

    case LCD_UI_ACTION:
      if (accept) ui->screen = LCD_UI_ITEMS;
      break;

    case LCD_UI_CLOCK:
      if (delta != 0) {
        if (ui->clock_field == 0U) ui->clock_hour = wrap_position((int32_t)ui->clock_hour + delta, 24U);
        else ui->clock_minute = wrap_position((int32_t)ui->clock_minute + delta, 60U);
        ui->clock_error = false;
      }
      if (accept) {
        if (ui->clock_field == 0U) ui->clock_field = 1U;
        else if (radio_clock_set(ui->clock_hour, ui->clock_minute)) ui->screen = LCD_UI_ITEMS;
        else ui->clock_error = true;
      }
      break;

    case LCD_UI_STATION_LIST: {
      const uint8_t count = (uint8_t)(app->settings.station_count + 2U);
      if (delta != 0) {
        ui->selected = wrap_position((int32_t)ui->selected + delta, count);
      }
      if (accept) {
        if (ui->selected == 0U) {
          ui->screen = LCD_UI_ITEMS;
          ui->category = station_category();
          ui->selected = 2U;
        } else if (ui->selected == 1U) {
          if (app->settings.station_count < RADIO_MAX_STATIONS) {
            ui->station_is_new = true;
            ui->station_frequency_khz = app->settings.frequency_khz;
            initialize_station_name(ui->station_name, "STACJA");
            ui->name_cursor = 0U;
            ui->name_editing = false;
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
            ui->name_editing = false;
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
          ui->name_editing = false;
          ui->screen = LCD_UI_STATION_NAME;
        } else {
          radio_app_station_update(app, ui->station_index,
                                   ui->station_frequency_khz, NULL, now_ms);
          ui->selected = 0U;
          ui->screen = LCD_UI_STATION_ACTIONS;
        }
      }
      break;

    case LCD_UI_STATION_NAME: {
      const uint8_t positions = name_positions(ui);
      if (delta != 0) {
        if (ui->name_editing) adjust_station_character(ui, delta);
        else ui->name_cursor = wrap_position((int32_t)ui->name_cursor + delta, (uint8_t)(positions + 2U));
      }
      if (accept) {
        if (ui->name_editing) {
          ui->name_editing = false;
          ui->name_cursor = wrap_position((int32_t)ui->name_cursor + 1U,
                                          (uint8_t)(name_positions(ui) + 2U));
        } else if (ui->name_cursor < positions) {
          ui->name_editing = true;
        } else if (ui->name_cursor == positions) {
          uint8_t length = RADIO_STATION_NAME_LENGTH;
          while (length > 0U && ui->station_name[length - 1U] == ' ') --length;
          ui->station_name[length] = '\0';
          if (ui->station_is_new) {
            if (radio_app_station_add(app, ui->station_frequency_khz, ui->station_name, now_ms)) {
              ui->station_index = (uint8_t)(app->settings.station_count - 1U);
              ui->station_is_new = false;
              ui->selected = 0U;
              ui->screen = LCD_UI_STATION_ACTIONS;
            }
          } else if (ui->station_index < app->settings.station_count) {
            radio_app_station_update(app, ui->station_index,
                                     app->settings.stations[ui->station_index].frequency_khz,
                                     ui->station_name, now_ms);
            ui->selected = 0U;
            ui->screen = LCD_UI_STATION_ACTIONS;
          }
        } else {
          ui->screen = ui->station_is_new ? LCD_UI_ITEMS : LCD_UI_STATION_ACTIONS;
          ui->selected = ui->station_is_new ? 1U : 3U;
        }
      }
      break;
    }

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
  if (delta != 0 || accept) ui->menu_scroll_started_ms = now_ms;
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
    case LCD_UI_HOME: lcd_home_render(ui, app, now_ms); break;
    case LCD_UI_CATEGORIES: render_categories(ui, now_ms); break;
    case LCD_UI_ITEMS: render_items(ui, app, now_ms); break;
    case LCD_UI_EDIT: render_edit(ui, app, now_ms); break;
    case LCD_UI_DEFAULTS: render_defaults(ui, now_ms); break;
    case LCD_UI_ACTION: render_action(ui, app, now_ms); break;
    case LCD_UI_CLOCK: render_clock(ui); break;
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

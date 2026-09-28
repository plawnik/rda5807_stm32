#include "lcd_ui.h"

#include "radio_clock.h"

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

#define TINY_GLYPH(a, b, c, d) \
  ((uint16_t)(((uint16_t)(a) << 9) | ((uint16_t)(b) << 6) | \
              ((uint16_t)(c) << 3) | (uint16_t)(d)))

static const uint16_t tiny_digits[10] = {
    TINY_GLYPH(7, 5, 5, 7), TINY_GLYPH(2, 6, 2, 7),
    TINY_GLYPH(6, 1, 2, 7), TINY_GLYPH(6, 1, 3, 6),
    TINY_GLYPH(5, 5, 7, 1), TINY_GLYPH(7, 4, 3, 6),
    TINY_GLYPH(3, 4, 7, 7), TINY_GLYPH(7, 1, 2, 2),
    TINY_GLYPH(7, 5, 7, 7), TINY_GLYPH(7, 7, 1, 6)};

static const uint16_t tiny_letters[26] = {
    TINY_GLYPH(2, 5, 7, 5), TINY_GLYPH(6, 5, 6, 7),
    TINY_GLYPH(3, 4, 4, 3), TINY_GLYPH(6, 5, 5, 6),
    TINY_GLYPH(7, 6, 4, 7), TINY_GLYPH(7, 6, 4, 4),
    TINY_GLYPH(3, 4, 5, 3), TINY_GLYPH(5, 5, 7, 5),
    TINY_GLYPH(7, 2, 2, 7), TINY_GLYPH(1, 1, 5, 2),
    TINY_GLYPH(5, 6, 6, 5), TINY_GLYPH(4, 4, 4, 7),
    TINY_GLYPH(5, 7, 7, 5), TINY_GLYPH(5, 7, 7, 5),
    TINY_GLYPH(7, 5, 5, 7), TINY_GLYPH(7, 5, 7, 4),
    TINY_GLYPH(7, 5, 7, 1), TINY_GLYPH(6, 5, 6, 5),
    TINY_GLYPH(7, 4, 3, 6), TINY_GLYPH(7, 2, 2, 2),
    TINY_GLYPH(5, 5, 5, 7), TINY_GLYPH(5, 5, 5, 2),
    TINY_GLYPH(5, 7, 7, 5), TINY_GLYPH(5, 2, 2, 5),
    TINY_GLYPH(5, 2, 2, 2), TINY_GLYPH(7, 1, 4, 7)};

static uint16_t tiny_glyph(char character) {
  if (character >= 'a' && character <= 'z') {
    character = (char)(character - 'a' + 'A');
  }
  if (character >= '0' && character <= '9') {
    return tiny_digits[(uint8_t)(character - '0')];
  }
  if (character >= 'A' && character <= 'Z') {
    return tiny_letters[(uint8_t)(character - 'A')];
  }
  switch (character) {
    case ':': return TINY_GLYPH(0, 2, 0, 2);
    case '.': return TINY_GLYPH(0, 0, 0, 2);
    case '-': return TINY_GLYPH(0, 0, 7, 0);
    case '/': return TINY_GLYPH(1, 1, 2, 4);
    case '_': return TINY_GLYPH(0, 0, 0, 7);
    case '+': return TINY_GLYPH(0, 2, 7, 2);
    default: return 0U;
  }
}

static uint8_t tiny_advance(char character) {
  if (character == ' ') return 3U;
  if (character == ':' || character == '.') return 2U;
  return 4U;
}

static uint16_t tiny_text_width(const char *text) {
  uint16_t width = 0U;
  if (text == NULL || text[0] == '\0') return 0U;
  while (*text != '\0') width = (uint16_t)(width + tiny_advance(*text++));
  return width == 0U ? 0U : (uint16_t)(width - 1U);
}

static int16_t draw_tiny_text(pcd8544_t *lcd, int16_t x, int16_t y,
                              const char *text) {
  if (lcd == NULL || text == NULL) return x;
  while (*text != '\0') {
    const uint16_t glyph = tiny_glyph(*text);
    for (uint8_t row = 0U; row < 4U; ++row) {
      const uint8_t pixels =
          (uint8_t)((glyph >> ((3U - row) * 3U)) & 0x07U);
      for (uint8_t column = 0U; column < 3U; ++column) {
        if ((pixels & (1U << (2U - column))) != 0U) {
          pcd8544_set_pixel(lcd, (int16_t)(x + column),
                            (int16_t)(y + row), true);
        }
      }
    }
    x = (int16_t)(x + tiny_advance(*text++));
  }
  return x;
}

static void draw_tiny_centered(pcd8544_t *lcd, int16_t x, int16_t y,
                               int16_t width, const char *text) {
  const uint16_t text_width = tiny_text_width(text);
  const int16_t offset = text_width < (uint16_t)width
                             ? (int16_t)((width - text_width) / 2)
                             : 0;
  draw_tiny_text(lcd, (int16_t)(x + offset), y, text);
}

static uint8_t scaled_rssi(uint8_t rssi) {
  return (uint8_t)(((uint16_t)rssi * 99U + 63U) / 127U);
}

static uint8_t rssi_bar_count(uint8_t value) {
  static const uint8_t thresholds[8] = {8U, 18U, 28U, 38U,
                                         48U, 58U, 72U, 88U};
  uint8_t bars = 0U;
  while (bars < 8U && value >= thresholds[bars]) ++bars;
  return bars;
}

static void draw_side_meter(pcd8544_t *lcd, bool right, uint8_t bars) {
  static const uint8_t y[8] = {32U, 27U, 22U, 17U, 12U, 7U, 2U, 0U};
  static const uint8_t height[8] = {4U, 4U, 4U, 4U, 4U, 4U, 4U, 1U};
  static const uint8_t width[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 7U};
  if (bars > 8U) bars = 8U;
  for (uint8_t level = 0U; level < bars; ++level) {
    const int16_t x = right ? (int16_t)(84U - width[level]) : 0;
    pcd8544_fill_rect(lcd, x, y[level], width[level], height[level], true);
  }
}

static void draw_signal_status_icon(pcd8544_t *lcd, bool station_valid) {
  static const uint8_t rows[6] = {0x3EU, 0x2AU, 0x1CU,
                                  0x08U, 0x0BU, 0x0BU};
  for (uint8_t row = 0U; row < 6U; ++row) {
    uint8_t pixels = rows[row];
    if (!station_valid && row >= 4U) pixels &= (uint8_t)~0x03U;
    for (uint8_t column = 0U; column < 6U; ++column) {
      if ((pixels & (1U << (5U - column))) != 0U) {
        pcd8544_set_pixel(lcd, column, (int16_t)(37U + row), true);
      }
    }
  }
}

static void draw_volume_status_icon(pcd8544_t *lcd, bool muted) {
  static const uint8_t speaker[5] = {0x02U, 0x06U, 0x06U, 0x06U, 0x02U};
  static const uint8_t waves[5] = {0x02U, 0x01U, 0x05U, 0x01U, 0x02U};
  static const uint8_t crossed[5] = {0x00U, 0x05U, 0x02U, 0x05U, 0x00U};
  for (uint8_t row = 0U; row < 5U; ++row) {
    const uint8_t right = muted ? crossed[row] : waves[row];
    const uint8_t pixels = (uint8_t)((speaker[row] << 3) | right);
    for (uint8_t column = 0U; column < 6U; ++column) {
      if ((pixels & (1U << (5U - column))) != 0U) {
        pcd8544_set_pixel(lcd, (int16_t)(78U + column),
                          (int16_t)(38U + row), true);
      }
    }
  }
}

static void draw_frequency(pcd8544_t *lcd, uint32_t frequency_khz) {
  char text[16];
  const uint32_t hundredths = (frequency_khz + 5U) / 10U;
  const uint32_t whole = hundredths / 100U;
  const uint32_t fraction = hundredths % 100U;
  int16_t x;
  snprintf(text, sizeof(text), "%lu.%02luMHz", (unsigned long)whole,
           (unsigned long)fraction);
  x = (int16_t)(10 + (64 - ((int16_t)strlen(text) * 6 - 1)) / 2);
  pcd8544_text(lcd, x, 11, text, 1U, true);
}

static const char *short_program_type(uint8_t type) {
  static const char *const names[32] = {
      "",       "INFO",  "PUB",    "INFO",  "SPORT", "EDUK",
      "TEATR",  "KULT",  "NAUKA",  "ROZR",  "POP",   "ROCK",
      "LEKKA",  "KLAS",  "KLAS",   "INNA",  "POGODA", "GOSP",
      "DZIECI", "SPOL",  "RELIGIA", "LIVE", "PODR",  "HOBBY",
      "JAZZ",   "INNA",  "INNA",   "HITY",  "FOLK",  "DOK",
      "TEST",   "ALARM"};
  return names[type & 0x1FU];
}

static void draw_no_rds_logo(pcd8544_t *lcd, uint8_t variant) {
  if ((variant & 1U) == 0U) {
    pcd8544_line(lcd, 22, 39, 29, 19, true);
    pcd8544_line(lcd, 29, 19, 36, 39, true);
    pcd8544_line(lcd, 25, 31, 33, 31, true);
    pcd8544_fill_rect(lcd, 28, 34, 3, 7, true);
    pcd8544_line(lcd, 18, 23, 23, 27, true);
    pcd8544_line(lcd, 18, 35, 23, 31, true);
    pcd8544_line(lcd, 40, 27, 45, 23, true);
    pcd8544_line(lcd, 40, 31, 45, 35, true);
    draw_tiny_text(lcd, 49, 29, "FM");
  } else {
    pcd8544_rect(lcd, 20, 24, 44, 17, true);
    pcd8544_line(lcd, 24, 23, 38, 19, true);
    pcd8544_line(lcd, 38, 19, 53, 19, true);
    pcd8544_rect(lcd, 24, 28, 13, 9, true);
    pcd8544_line(lcd, 26, 32, 30, 29, true);
    pcd8544_line(lcd, 30, 29, 35, 34, true);
    pcd8544_line(lcd, 35, 34, 31, 37, true);
    pcd8544_line(lcd, 31, 37, 26, 32, true);
    pcd8544_line(lcd, 42, 29, 58, 29, true);
    pcd8544_set_pixel(lcd, 47, 28, true);
    pcd8544_fill_rect(lcd, 43, 34, 3, 4, true);
    pcd8544_fill_rect(lcd, 49, 32, 3, 6, true);
    pcd8544_fill_rect(lcd, 55, 30, 3, 8, true);
  }
}

static void draw_control_mode(pcd8544_t *lcd, uint8_t action) {
  const char *label;
  int16_t x;
  if (action == RADIO_CONTROL_SEEK) label = "SEEK";
  else if (action == RADIO_CONTROL_STATIONS) label = "STACJA";
  else label = "TUNE";
  pcd8544_set_pixel(lcd, 13, 44, true);
  pcd8544_fill_rect(lcd, 12, 45, 2, 2, true);
  pcd8544_fill_rect(lcd, 11, 46, 3, 1, true);
  pcd8544_set_pixel(lcd, 13, 47, true);
  pcd8544_set_pixel(lcd, 71, 44, true);
  pcd8544_fill_rect(lcd, 71, 45, 2, 2, true);
  pcd8544_fill_rect(lcd, 71, 46, 3, 1, true);
  pcd8544_set_pixel(lcd, 71, 47, true);
  x = (int16_t)(11 + (63 - (int16_t)tiny_text_width(label)) / 2);
  draw_tiny_text(lcd, x, 44, label);
}

static void draw_clock_and_date(pcd8544_t *lcd) {
  static const char *const months[12] = {
      "STY", "LUT", "MAR", "KWI", "MAJ", "CZE",
      "LIP", "SIE", "WRZ", "PAZ", "LIS", "GRU"};
  radio_clock_time_t time;
  char text[8];
  if (!radio_clock_read(&time) || time.month == 0U || time.month > 12U) return;
  snprintf(text, sizeof(text), "%02u %s", time.day, months[time.month - 1U]);
  draw_tiny_centered(lcd, 33, 1, 22, text);
  snprintf(text, sizeof(text), "%02u:%02u", time.hour, time.minute);
  draw_tiny_centered(lcd, 59, 1, 18, text);
}

static void draw_rds_text(pcd8544_t *lcd, const rds_decoder_t *rds,
                          uint32_t now_ms) {
  char ps[9] = {0};
  char window[35] = {0};
  char first[18] = {0};
  char second[18] = {0};
  const char *pty = short_program_type(rds->program_type);
  const size_t pty_width = tiny_text_width(pty);
  size_t length;

  if (rds->ps_valid) {
    memcpy(ps, rds->program_service, 8U);
    draw_tiny_text(lcd, 8, 25, ps);
  }
  if (pty_width <= 35U) {
    draw_tiny_text(lcd, (int16_t)(41 + 35 - pty_width), 25, pty);
  }
  if (!rds->radio_text_valid) return;
  scrolling_text(window, sizeof(window), rds->radio_text, 64U, 34U, now_ms);
  length = strnlen(window, 34U);
  if (length > 17U) {
    memcpy(first, window, 17U);
    memcpy(second, &window[17], length - 17U);
  } else {
    memcpy(first, window, length);
  }
  draw_tiny_text(lcd, 8, 31, first);
  draw_tiny_text(lcd, 8, 37, second);
}

static void render_home(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char text[18];
  const radio_station_t *station;
  const rds_decoder_t *rds = &app->rds;
  const rda5807_status_t *status = &app->radio.status;
  const uint8_t rssi = scaled_rssi(radio_app_display_rssi(app));
  const bool has_rds = status->rds_synchronized || rds->ps_valid ||
                       rds->radio_text_valid;
  const uint32_t logo_slot = now_ms / 30000U;

  if (!app->radio_available) {
    pcd8544_text(ui->lcd, 12, 7, "BRAK RADIA", 1U, true);
    pcd8544_text(ui->lcd, 9, 22, "Sprawdz I2C", 1U, true);
    pcd8544_text(ui->lcd, 9, 36, "PB10 / PB11", 1U, true);
    return;
  }

  draw_side_meter(ui->lcd, false, rssi_bar_count(rssi));
  draw_side_meter(ui->lcd, true,
                  app->settings.volume == 0U
                      ? 0U
                      : (uint8_t)(((uint16_t)app->settings.volume * 8U + 14U) /
                                  15U));
  draw_signal_status_icon(ui->lcd, status->station_valid);
  draw_volume_status_icon(ui->lcd,
                          app->settings.muted || app->settings.volume == 0U);
  snprintf(text, sizeof(text), "%02u", rssi);
  draw_tiny_centered(ui->lcd, 0, 44, 8, text);
  snprintf(text, sizeof(text), "%u", app->settings.volume);
  draw_tiny_centered(ui->lcd, 77, 44, 7, text);

  if (status->rds_synchronized) draw_tiny_centered(ui->lcd, 8, 1, 20, "RDS");
  if (app->settings.bass_boost) draw_tiny_text(ui->lcd, 29, 1, "B");
  draw_clock_and_date(ui->lcd);

  station = radio_app_current_station(app);
  if (station != NULL) {
    scrolling_text(text, sizeof(text), station->name,
                   RADIO_STATION_NAME_LENGTH, 17U, now_ms);
  } else {
    strcpy(text, "FM");
  }
  draw_tiny_centered(ui->lcd, 8, 6, 68, text);
  draw_frequency(ui->lcd, app->settings.frequency_khz);

  if (has_rds) {
    draw_rds_text(ui->lcd, rds, now_ms);
  } else {
    if (ui->logo_slot != logo_slot) {
      if (ui->logo_slot == UINT32_MAX) {
        ui->logo_variant =
            (uint8_t)((app->settings.frequency_khz ^ now_ms) & 1U);
      } else {
        ui->logo_variant ^= 1U;
      }
      ui->logo_slot = logo_slot;
    }
    draw_no_rds_logo(ui->lcd, ui->logo_variant);
  }
  draw_control_mode(ui->lcd, ui->home_action);
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

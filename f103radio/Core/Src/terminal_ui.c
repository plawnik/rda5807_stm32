#include "terminal_ui.h"

#include "uart_debug.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_RESET       "\x1b[0m"
#define ANSI_CYAN        "\x1b[36m"
#define ANSI_GREEN       "\x1b[32m"
#define ANSI_YELLOW      "\x1b[33m"
#define ANSI_RED         "\x1b[31m"
#define ANSI_BRIGHT_WHITE "\x1b[97m"
#define ANSI_DIM         "\x1b[2m"
#define ANSI_SELECTED    "\x1b[30;46m"
#define ANSI_DIGIT       "\x1b[30;43m"
#define TERMINAL_RENDER_INTERVAL_MS 100U
#define TERMINAL_ESCAPE_TIMEOUT_MS 60U
#define TERMINAL_FREQUENCY_EDIT_TIMEOUT_MS 5000U
#define TERMINAL_MENU_ROWS 12U
#define TERMINAL_SCREEN_WIDTH 112U
#define TERMINAL_FIRST_MENU_ROW 21U
#define TERMINAL_ROW_BUFFER_SIZE 512U

typedef enum {
  KEY_NONE = 0,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_ENTER,
  KEY_ESCAPE,
  KEY_REFRESH,
  KEY_MUTE,
  KEY_FREQUENCY,
  KEY_OPTIONS,
  KEY_STEREO,
  KEY_DIGIT
} key_code_t;

typedef struct {
  key_code_t code;
  uint8_t digit;
} key_event_t;

static const uint8_t big_digits[10][7] = {
    {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU},
    {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU},
    {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU},
    {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU},
    {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU},
    {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU},
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U},
    {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU},
    {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU}};

static const uint8_t small_digits[10][5] = {
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

static terminal_ui_t *active_terminal;

#define TERMINAL_MENU_EXTENDED_RANGE ((radio_menu_item_t)RADIO_MENU_COUNT)

static const radio_menu_item_t terminal_menu_items[] = {
    RADIO_MENU_FREQUENCY,
    TERMINAL_MENU_EXTENDED_RANGE,
    RADIO_MENU_VOLUME,
    RADIO_MENU_MUTE,
    RADIO_MENU_AUDIO_MODE,
    RADIO_MENU_BASS,
    RADIO_MENU_SEEK_UP,
    RADIO_MENU_SEEK_DOWN,
    RADIO_MENU_RDS,
    RADIO_MENU_RBDS,
    RADIO_MENU_DEEMPHASIS,
    RADIO_MENU_SEEK_LIMIT,
    RADIO_MENU_SEEK_ALGORITHM,
    RADIO_MENU_SEEK_THRESHOLD,
    RADIO_MENU_OLD_SEEK_THRESHOLD,
    RADIO_MENU_SOFTMUTE,
    RADIO_MENU_SOFTBLEND,
    RADIO_MENU_SOFTBLEND_THRESHOLD,
    RADIO_MENU_AFC,
    RADIO_MENU_NEW_METHOD,
    RADIO_MENU_LNA_INPUT,
    RADIO_MENU_LNA_CURRENT,
    RADIO_MENU_LCD_CONTRAST,
    RADIO_MENU_LCD_INVERT,
    RADIO_MENU_LCD_BACKLIGHT,
    RADIO_MENU_DEFAULTS};

#define TERMINAL_MENU_ITEM_COUNT \
  ((uint8_t)(sizeof(terminal_menu_items) / sizeof(terminal_menu_items[0])))

static bool dequeue(terminal_ui_t *ui, uint8_t *value) {
  if (ui->rx_tail == ui->rx_head) return false;
  *value = ui->rx_buffer[ui->rx_tail];
  ui->rx_tail = (uint8_t)((ui->rx_tail + 1U) &
                          (TERMINAL_RX_BUFFER_SIZE - 1U));
  return true;
}

static key_event_t parse_byte(terminal_ui_t *ui, uint8_t value,
                              uint32_t now_ms) {
  key_event_t event = {KEY_NONE, 0U};
  ui->parser_changed_ms = now_ms;
  if (ui->parser_state == 0U) {
    if (value == 0x1BU) ui->parser_state = 1U;
    else if (value == '\r') event.code = KEY_ENTER;
    else if (value == '\n') event.code = KEY_NONE;
    else if (value == 'k' || value == 'K') event.code = KEY_UP;
    else if (value == 'j' || value == 'J') event.code = KEY_DOWN;
    else if (value == 'h' || value == 'H' || value == '-') event.code = KEY_LEFT;
    else if (value == 'l' || value == 'L' || value == '+') event.code = KEY_RIGHT;
    else if (value == 'r' || value == 'R') event.code = KEY_REFRESH;
    else if (value == 'm' || value == 'M') event.code = KEY_MUTE;
    else if (value == 'p' || value == 'P') event.code = KEY_FREQUENCY;
    else if (value == 'o' || value == 'O') event.code = KEY_OPTIONS;
    else if (value == 's' || value == 'S') event.code = KEY_STEREO;
    else if (isdigit((int)value)) {
      event.code = KEY_DIGIT;
      event.digit = (uint8_t)(value - '0');
    }
  } else if (ui->parser_state == 1U) {
    if (value == '[' || value == 'O') ui->parser_state = 2U;
    else {
      ui->parser_state = 0U;
      event.code = KEY_ESCAPE;
    }
  } else {
    ui->parser_state = 0U;
    if (value == 'A') event.code = KEY_UP;
    else if (value == 'B') event.code = KEY_DOWN;
    else if (value == 'C') event.code = KEY_RIGHT;
    else if (value == 'D') event.code = KEY_LEFT;
  }
  return event;
}

static void format_frequency_digits(uint32_t frequency_khz, char digits[6]) {
  uint32_t hundredths_mhz = (frequency_khz + 5U) / 10U;
  if (hundredths_mhz > 99999U) hundredths_mhz = 99999U;
  for (int8_t index = 4; index >= 0; --index) {
    digits[index] = (char)('0' + (hundredths_mhz % 10U));
    hundredths_mhz /= 10U;
  }
  digits[5] = '\0';
}

static void apply_frequency_digit(terminal_ui_t *ui, radio_app_t *app,
                                  uint8_t digit, bool advance,
                                  uint32_t now_ms) {
  char digits[6];
  uint32_t frequency;
  format_frequency_digits(app->settings.frequency_khz, digits);
  digits[ui->frequency_digit] = (char)('0' + digit);
  frequency = (uint32_t)strtoul(digits, NULL, 10) * 10U;
  radio_app_set_frequency_auto(app, (int32_t)frequency, 50U, now_ms);
  if (advance && ui->frequency_digit < 4U) ++ui->frequency_digit;
}

static void edit_frequency(terminal_ui_t *ui, radio_app_t *app,
                           key_event_t event, uint32_t now_ms) {
  static const int32_t deltas_khz[5] = {100000, 10000, 1000, 100, 50};
  int32_t target;
  if (event.code == KEY_LEFT) {
    if (ui->frequency_digit > 0U) --ui->frequency_digit;
  } else if (event.code == KEY_RIGHT) {
    if (ui->frequency_digit < 4U) ++ui->frequency_digit;
  } else if (event.code == KEY_UP || event.code == KEY_DOWN) {
    target = (int32_t)app->settings.frequency_khz;
    if (event.code == KEY_UP) {
      target += deltas_khz[ui->frequency_digit];
    } else {
      target -= deltas_khz[ui->frequency_digit];
      --target; /* Break an exact half-grid tie in the requested direction. */
    }
    radio_app_set_frequency_auto(
        app, target,
        ui->frequency_digit == 4U || target % 100 != 0 ? 50U : 100U,
        now_ms);
  } else if (event.code == KEY_DIGIT) {
    apply_frequency_digit(ui, app, event.digit, true, now_ms);
  }
}

static void enter_frequency_mode(terminal_ui_t *ui, const radio_app_t *app,
                                 uint32_t now_ms) {
  ui->mode = TERMINAL_UI_FREQUENCY;
  (void)app;
  ui->frequency_digit = 3U;
  ui->last_interaction_ms = now_ms;
}

static uint8_t terminal_menu_index(radio_menu_item_t item) {
  for (uint8_t index = 0U; index < TERMINAL_MENU_ITEM_COUNT; ++index) {
    if (terminal_menu_items[index] == item) return index;
  }
  return 0U;
}

static void move_menu_selection(terminal_ui_t *ui, int8_t delta) {
  int16_t index = terminal_menu_index(ui->selected);
  index += delta;
  if (index < 0) index = TERMINAL_MENU_ITEM_COUNT - 1U;
  if (index >= TERMINAL_MENU_ITEM_COUNT) index = 0;
  ui->selected = terminal_menu_items[index];
}

static void handle_key(terminal_ui_t *ui, radio_app_t *app,
                       key_event_t event, uint32_t now_ms) {
  if (event.code == KEY_NONE) return;
  ui->force_render = true;
  ui->last_interaction_ms = now_ms;

  if (event.code == KEY_REFRESH) {
    ui->redraw_all = true;
    return;
  }
  if (event.code == KEY_MUTE) {
    radio_app_toggle_mute(app, now_ms);
    return;
  }
  if (event.code == KEY_FREQUENCY) {
    if (ui->mode == TERMINAL_UI_FREQUENCY) {
      ui->mode = TERMINAL_UI_HOME;
    } else {
      enter_frequency_mode(ui, app, now_ms);
    }
    return;
  }
  if (event.code == KEY_OPTIONS) {
    if (ui->mode == TERMINAL_UI_MENU ||
        ui->mode == TERMINAL_UI_MENU_EDIT) {
      ui->mode = TERMINAL_UI_HOME;
    } else {
      ui->mode = TERMINAL_UI_MENU;
    }
    return;
  }

  if (ui->mode == TERMINAL_UI_HOME) {
    if (event.code == KEY_UP) {
      radio_app_menu_adjust(app, RADIO_MENU_VOLUME, 1, now_ms);
    } else if (event.code == KEY_DOWN) {
      radio_app_menu_adjust(app, RADIO_MENU_VOLUME, -1, now_ms);
    } else if (event.code == KEY_LEFT || event.code == KEY_RIGHT) {
      radio_app_seek(app, event.code == KEY_RIGHT, now_ms);
    } else if (event.code == KEY_STEREO) {
      radio_app_menu_adjust(app, RADIO_MENU_AUDIO_MODE, 1, now_ms);
    }
    return;
  }

  if (ui->mode == TERMINAL_UI_FREQUENCY) {
    if (event.code == KEY_ENTER || event.code == KEY_ESCAPE) {
      ui->mode = TERMINAL_UI_HOME;
      return;
    }
    edit_frequency(ui, app, event, now_ms);
    return;
  }

  if (ui->mode == TERMINAL_UI_MENU) {
    if (event.code == KEY_ESCAPE) {
      ui->mode = TERMINAL_UI_HOME;
    } else if (event.code == KEY_UP) {
      move_menu_selection(ui, -1);
    } else if (event.code == KEY_DOWN) {
      move_menu_selection(ui, 1);
    } else if (event.code == KEY_ENTER || event.code == KEY_RIGHT) {
      if (ui->selected == RADIO_MENU_FREQUENCY) {
        enter_frequency_mode(ui, app, now_ms);
      } else if (radio_app_menu_is_action(ui->selected)) {
        radio_app_menu_activate(app, ui->selected, now_ms);
      } else {
        ui->mode = TERMINAL_UI_MENU_EDIT;
      }
    }
    return;
  }

  if (event.code == KEY_ENTER || event.code == KEY_ESCAPE) {
    ui->mode = TERMINAL_UI_MENU;
  } else if (event.code == KEY_LEFT || event.code == KEY_DOWN) {
    if (ui->selected == TERMINAL_MENU_EXTENDED_RANGE) {
      radio_app_set_extended_tuning(app, !app->settings.extended_tuning,
                                    now_ms);
    } else {
      radio_app_menu_adjust(app, ui->selected, -1, now_ms);
    }
  } else if (event.code == KEY_RIGHT || event.code == KEY_UP) {
    if (ui->selected == TERMINAL_MENU_EXTENDED_RANGE) {
      radio_app_set_extended_tuning(app, !app->settings.extended_tuning,
                                    now_ms);
    } else {
      radio_app_menu_adjust(app, ui->selected, 1, now_ms);
    }
  }
}

static uint32_t text_hash(const char *text) {
  uint32_t hash = 2166136261UL;
  while (*text != '\0') {
    hash ^= (uint8_t)*text++;
    hash *= 16777619UL;
  }
  return hash;
}

static void write_row(terminal_ui_t *ui, uint8_t row, const char *text) {
  const uint32_t hash = text_hash(text);
  if (row == 0U || row > TERMINAL_SCREEN_ROWS) return;
  if (!ui->redraw_all && ui->row_hashes[row - 1U] == hash) return;
  uart_debug_printf("\x1b[%u;1H", row);
  uart_debug_write(text);
  uart_debug_write(ANSI_RESET "\x1b[K");
  ui->row_hashes[row - 1U] = hash;
}

static void write_rowf(terminal_ui_t *ui, uint8_t row, const char *format,
                       ...) {
  char buffer[TERMINAL_ROW_BUFFER_SIZE];
  va_list arguments;
  va_start(arguments, format);
  vsnprintf(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  write_row(ui, row, buffer);
}

static void make_border(char border[TERMINAL_SCREEN_WIDTH + 1U]) {
  border[0] = '+';
  memset(&border[1], '-', TERMINAL_SCREEN_WIDTH - 2U);
  border[TERMINAL_SCREEN_WIDTH - 1U] = '+';
  border[TERMINAL_SCREEN_WIDTH] = '\0';
}

static void trim_copy(char *output, size_t output_size, const char *input,
                      size_t input_limit) {
  size_t length = strnlen(input, input_limit);
  while (length > 0U && input[length - 1U] == ' ') --length;
  if (length >= output_size) length = output_size - 1U;
  memcpy(output, input, length);
  output[length] = '\0';
}

static void field_clear(char *field, size_t width) {
  memset(field, ' ', width);
  field[width] = '\0';
}

static void field_text(char *field, size_t width, size_t position,
                       const char *text) {
  size_t length;
  if (position >= width || text == NULL) return;
  length = strlen(text);
  if (length > width - position) length = width - position;
  memcpy(&field[position], text, length);
}

static void field_center(char *field, size_t width, const char *text) {
  size_t length;
  if (text == NULL) return;
  length = strlen(text);
  field_text(field, width, length < width ? (width - length) / 2U : 0U, text);
}

static void build_level_panel(char *output, size_t width, uint8_t art_row,
                              uint8_t value, uint8_t maximum,
                              const char *label, uint8_t digit_count) {
  char number[4];
  char footer[20];
  uint8_t levels;
  size_t digit_start;
  field_clear(output, width);
  if (art_row == 0U) {
    field_center(output, width, label);
    return;
  }
  if (art_row >= 1U && art_row <= 5U) {
    levels = (uint8_t)(((uint16_t)value * 5U + maximum - 1U) / maximum);
    output[1] = '[';
    output[2] = (5U - art_row) < levels ? '#' : '.';
    output[3] = (5U - art_row) < levels ? '#' : '.';
    output[4] = ']';
    snprintf(number, sizeof(number), digit_count == 2U ? "%02u" : "%03u",
             value);
    digit_start = digit_count == 2U ? 7U : 6U;
    for (uint8_t index = 0U; index < digit_count; ++index) {
      const uint8_t glyph = (uint8_t)(number[index] - '0');
      const uint8_t bits = small_digits[glyph][art_row - 1U];
      for (uint8_t column = 0U; column < 3U; ++column) {
        output[digit_start + index * 4U + column] =
            (bits & (1U << (2U - column))) != 0U ? '#' : ' ';
      }
    }
    return;
  }
  if (digit_count == 2U) {
    snprintf(footer, sizeof(footer), "%02u / %02u", value, maximum);
  } else {
    const uint8_t percent =
        (uint8_t)(((uint16_t)value * 100U + maximum / 2U) / maximum);
    snprintf(footer, sizeof(footer), "%03u/%03u %3u%%", value, maximum,
             percent);
  }
  field_center(output, width, footer);
}

static void append_text(char *output, size_t output_size, size_t *position,
                        const char *text) {
  size_t length;
  if (*position >= output_size - 1U) return;
  length = strlen(text);
  if (length > output_size - 1U - *position) {
    length = output_size - 1U - *position;
  }
  memcpy(&output[*position], text, length);
  *position += length;
  output[*position] = '\0';
}

static void append_repeat(char *output, size_t output_size, size_t *position,
                          char character, size_t count) {
  while (count-- > 0U && *position < output_size - 1U) {
    output[(*position)++] = character;
  }
  output[*position] = '\0';
}

static void expand_pixel_art(const char *input, char *output,
                             size_t output_size) {
  size_t position = 0U;
  output[0] = '\0';
  while (*input != '\0' && position < output_size - 1U) {
    if (*input == '#') {
      append_text(output, output_size, &position, "\xE2\x96\x88");
    } else {
      output[position++] = *input;
      output[position] = '\0';
    }
    ++input;
  }
}

static void build_frequency_row(const terminal_ui_t *ui,
                                const radio_app_t *app, uint8_t art_row,
                                char *output, size_t output_size) {
  char digits[6];
  size_t position = 0U;
  size_t visible = 0U;
  const size_t width = 72U;
  const size_t prefix = 18U;
  const bool frequency_edit = ui->mode == TERMINAL_UI_FREQUENCY;
  format_frequency_digits(app->settings.frequency_khz, digits);
  output[0] = '\0';
  append_repeat(output, output_size, &position, ' ', prefix);
  visible += prefix;
  append_text(output, output_size, &position, ANSI_BRIGHT_WHITE);
  for (uint8_t index = 0U; index < 5U; ++index) {
    const uint8_t glyph = (uint8_t)(digits[index] - '0');
    const uint8_t bits = big_digits[glyph][art_row];
    const bool selected = frequency_edit && index == ui->frequency_digit;
    if (selected) append_text(output, output_size, &position, ANSI_DIGIT);
    for (uint8_t column = 0U; column < 5U; ++column) {
      append_repeat(output, output_size, &position,
                    (bits & (1U << (4U - column))) != 0U
                        ? '#'
                        : ' ',
                    1U);
    }
    if (selected) {
      append_text(output, output_size, &position, ANSI_BRIGHT_WHITE);
    }
    append_repeat(output, output_size, &position, ' ', 1U);
    visible += 6U;
    if (index == 2U) {
      append_repeat(output, output_size, &position,
                    art_row >= 5U ? '#' : ' ', 2U);
      visible += 2U;
    }
  }
  if (art_row == 3U) {
    append_text(output, output_size, &position, ANSI_CYAN);
    append_text(output, output_size, &position, " MHz");
    append_text(output, output_size, &position, ANSI_BRIGHT_WHITE);
    visible += 4U;
  }
  append_text(output, output_size, &position, ANSI_RESET);
  if (visible < width) {
    append_repeat(output, output_size, &position, ' ', width - visible);
  }
}

static const char *operation_name(rda5807_operation_t operation) {
  switch (operation) {
    case RDA5807_OPERATION_TUNING: return "STROJENIE";
    case RDA5807_OPERATION_SEEKING_UP: return "SEEK >>";
    case RDA5807_OPERATION_SEEKING_DOWN: return "<< SEEK";
    case RDA5807_OPERATION_IDLE:
    default: return "GOTOWY";
  }
}

static const char *mode_name(terminal_ui_mode_t mode) {
  switch (mode) {
    case TERMINAL_UI_FREQUENCY: return "EDYCJA CZESTOTLIWOSCI";
    case TERMINAL_UI_MENU: return "WYBOR OPCJI";
    case TERMINAL_UI_MENU_EDIT: return "EDYCJA OPCJI";
    case TERMINAL_UI_HOME:
    default: return "PODGLAD";
  }
}

static void render_dashboard(terminal_ui_t *ui, const radio_app_t *app) {
  char left[19];
  char center[256];
  char right[21];
  char raw_line[384];
  char line[TERMINAL_ROW_BUFFER_SIZE];
  for (uint8_t row = 0U; row < 7U; ++row) {
    build_level_panel(left, 18U, row, app->settings.volume, 15U,
                      "GLOSNOSC", 2U);
    build_frequency_row(ui, app, row, center, sizeof(center));
    build_level_panel(right, 20U, row, app->radio.status.rssi, 127U,
                      "SYGNAL RSSI", 3U);
    snprintf(raw_line, sizeof(raw_line), ANSI_GREEN "%s" ANSI_RESET "%s"
             ANSI_CYAN "%s" ANSI_RESET, left, center, right);
    expand_pixel_art(raw_line, line, sizeof(line));
    write_row(ui, (uint8_t)(5U + row), line);
  }
}

static void render_menu(terminal_ui_t *ui, const radio_app_t *app) {
  char value[40];
  const uint8_t selected_index = terminal_menu_index(ui->selected);
  int16_t first = (int16_t)selected_index - TERMINAL_MENU_ROWS / 2;
  const bool menu_active =
      ui->mode == TERMINAL_UI_MENU || ui->mode == TERMINAL_UI_MENU_EDIT;
  if (first < 0) first = 0;
  if (first > (int16_t)TERMINAL_MENU_ITEM_COUNT -
                  (int16_t)TERMINAL_MENU_ROWS) {
    first = (int16_t)TERMINAL_MENU_ITEM_COUNT -
            (int16_t)TERMINAL_MENU_ROWS;
  }

  for (uint8_t row = 0U; row < TERMINAL_MENU_ROWS; ++row) {
    const uint8_t item_index = (uint8_t)(first + row);
    const radio_menu_item_t item = terminal_menu_items[item_index];
    if (item == RADIO_MENU_FREQUENCY) {
      snprintf(value, sizeof(value), "%03lu.%02lu MHz",
               (unsigned long)(app->settings.frequency_khz / 1000U),
               (unsigned long)((app->settings.frequency_khz % 1000U) / 10U));
    } else if (item == TERMINAL_MENU_EXTENDED_RANGE) {
      snprintf(value, sizeof(value), "%s",
               app->settings.extended_tuning
                   ? "WL. (max 291.60 MHz)"
                   : "WYL. (max 115.00 MHz)");
    } else {
      radio_app_menu_value(app, item, value, sizeof(value));
    }
    if (menu_active && item == ui->selected) {
      const char *style = ui->mode == TERMINAL_UI_MENU_EDIT
                              ? ANSI_DIGIT
                              : ANSI_SELECTED;
      write_rowf(ui, (uint8_t)(TERMINAL_FIRST_MENU_ROW + row),
                 "%s> %02u  %-32s %-34s  %s%s", style,
                 (unsigned)item_index + 1U,
                 item == TERMINAL_MENU_EXTENDED_RANGE
                     ? "Zakres rozszerzony"
                     : radio_app_menu_label(item),
                 value,
                 ui->mode == TERMINAL_UI_MENU_EDIT ? "EDYCJA" : "WYBRANE",
                 ANSI_RESET);
    } else {
      write_rowf(ui, (uint8_t)(TERMINAL_FIRST_MENU_ROW + row),
                 "  %02u  %-32s %-34s",
                 (unsigned)item_index + 1U,
                 item == TERMINAL_MENU_EXTENDED_RANGE
                     ? "Zakres rozszerzony"
                     : radio_app_menu_label(item),
                 value);
    }
  }
}

void terminal_ui_init(terminal_ui_t *ui, UART_HandleTypeDef *uart,
                      uint32_t now_ms) {
  if (ui == NULL || uart == NULL) return;
  memset(ui, 0, sizeof(*ui));
  ui->uart = uart;
  ui->parser_changed_ms = now_ms;
  ui->last_interaction_ms = now_ms;
  ui->frequency_digit = 3U;
  ui->mode = TERMINAL_UI_HOME;
  ui->force_render = true;
  ui->redraw_all = true;
  active_terminal = ui;
  uart_debug_init(uart);
  uart_debug_write("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l\x1b[?7l"
                   "\x1b]0;RDA5807 STM32\x07");
  HAL_UART_Receive_IT(uart, &ui->rx_byte, 1U);
}

void terminal_ui_uart_rx_complete(UART_HandleTypeDef *uart) {
  uint8_t next;
  terminal_ui_t *ui = active_terminal;
  if (ui == NULL || uart != ui->uart) return;
  next = (uint8_t)((ui->rx_head + 1U) & (TERMINAL_RX_BUFFER_SIZE - 1U));
  if (next != ui->rx_tail) {
    ui->rx_buffer[ui->rx_head] = ui->rx_byte;
    ui->rx_head = next;
  }
  HAL_UART_Receive_IT(ui->uart, &ui->rx_byte, 1U);
}

void terminal_ui_uart_error(UART_HandleTypeDef *uart) {
  terminal_ui_t *ui = active_terminal;
  if (ui == NULL || uart != ui->uart) return;
  HAL_UART_Receive_IT(ui->uart, &ui->rx_byte, 1U);
}

void terminal_ui_process(terminal_ui_t *ui, radio_app_t *app,
                         uint32_t now_ms) {
  uint8_t value;
  if (ui == NULL || app == NULL) return;
  while (dequeue(ui, &value)) {
    handle_key(ui, app, parse_byte(ui, value, now_ms), now_ms);
  }
  if (ui->parser_state == 1U &&
      (uint32_t)(now_ms - ui->parser_changed_ms) >=
          TERMINAL_ESCAPE_TIMEOUT_MS) {
    ui->parser_state = 0U;
    handle_key(ui, app, (key_event_t){KEY_ESCAPE, 0U}, now_ms);
  }
  if (ui->mode == TERMINAL_UI_FREQUENCY &&
      (uint32_t)(now_ms - ui->last_interaction_ms) >=
          TERMINAL_FREQUENCY_EDIT_TIMEOUT_MS) {
    ui->mode = TERMINAL_UI_HOME;
    ui->force_render = true;
  }
}

void terminal_ui_render(terminal_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char border[TERMINAL_SCREEN_WIDTH + 1U];
  char ps[16];
  char rt[65];
  const char *audio_mode;
  const char *rds_state;
  const char *station_state;
  const char *frequency_range;
  uint8_t rssi_percent;
  if (ui == NULL || app == NULL) return;
  if (!ui->force_render &&
      (uint32_t)(now_ms - ui->last_render_ms) <
          TERMINAL_RENDER_INTERVAL_MS) {
    return;
  }
  ui->force_render = false;
  ui->last_render_ms = now_ms;
  ui->rendered_revision = app->revision;

  if (ui->redraw_all) uart_debug_write("\x1b[2J");
  make_border(border);
  trim_copy(ps, sizeof(ps),
            app->rds.ps_valid ? app->rds.program_service : "--", 8U);
  trim_copy(rt, sizeof(rt), app->rds.radio_text, 64U);
  audio_mode =
      app->radio.status.stereo && !app->settings.force_mono ? "STEREO" : "MONO";
  rds_state = app->radio.status.rds_synchronized ? "SYNC" : "BRAK";
  station_state = app->radio.status.station_valid ? "STACJA" : "SZUM";
  frequency_range = app->settings.extended_tuning
                        ? "050.00-291.60 MHz (EKSP.)"
                        : "050.00-115.00 MHz";
  rssi_percent = (uint8_t)(((uint16_t)app->radio.status.rssi * 100U + 63U) /
                           127U);

  write_rowf(ui, 1U, ANSI_CYAN "%s" ANSI_RESET, border);
  write_rowf(ui, 2U,
             ANSI_CYAN "|" ANSI_RESET "  " ANSI_GREEN
             "RDA5807M / STM32F103 - ODBIORNIK FM" ANSI_RESET
             "                                      Tryb: "
             ANSI_YELLOW "%-23s" ANSI_RESET ANSI_CYAN "|" ANSI_RESET,
             mode_name(ui->mode));
  write_rowf(ui, 3U,
             "  %s[%s]%s AUDIO   %s[R]%s RDS:%-4s   %s[FM]%s %-6s   "
             "%s[%s]%s %-9s   %s[X]%s MUTE:%-3s   PS: %s%-8s%s",
             ANSI_GREEN, audio_mode, ANSI_RESET, ANSI_CYAN, ANSI_RESET,
             rds_state, ANSI_GREEN, ANSI_RESET, station_state, ANSI_YELLOW,
             app->radio.operation == RDA5807_OPERATION_IDLE ? "--" : ">>",
             ANSI_RESET, operation_name(app->radio.operation), ANSI_RED,
             ANSI_RESET, app->settings.muted ? "TAK" : "NIE", ANSI_GREEN,
             ps, ANSI_RESET);
  write_rowf(ui, 4U, ANSI_CYAN "%s" ANSI_RESET, border);
  render_dashboard(ui, app);
  write_rowf(ui, 12U,
             "  Stacja: " ANSI_GREEN "%-8s" ANSI_RESET
             "   PI: %04X   PTY: %-22s   TP:%-3s TA:%-3s   "
             "Audio: " ANSI_YELLOW "%s" ANSI_RESET,
             ps, app->rds.program_id,
             rds_program_type_name(app->rds.program_type),
             app->rds.traffic_program ? "TAK" : "NIE",
             app->rds.traffic_announcement ? "TAK" : "NIE", audio_mode);
  write_rowf(ui, 13U,
             "  RadioText: " ANSI_GREEN "%-94.94s" ANSI_RESET,
             rt[0] != '\0' ? rt : "--");
  if (app->rds.clock_valid) {
    write_rowf(ui, 14U,
               "  Czas RDS: %02u:%02u   MJD: %-6u   RDS:%-4s   "
               "FM:%-6s   Tuner:%-8s   Modul:%s",
               app->rds.local_hour, app->rds.local_minute,
               app->rds.modified_julian_day, rds_state, station_state,
               operation_name(app->radio.operation),
               app->radio_available ? ANSI_GREEN "ONLINE" ANSI_RESET
                                    : ANSI_RED "OFFLINE" ANSI_RESET);
  } else {
    write_rowf(ui, 14U,
               "  Czas RDS: --:--      MJD: --       RDS:%-4s   "
               "FM:%-6s   Tuner:%-8s   Modul:%s",
               rds_state, station_state, operation_name(app->radio.operation),
               app->radio_available ? ANSI_GREEN "ONLINE" ANSI_RESET
                                    : ANSI_RED "OFFLINE" ANSI_RESET);
  }
  write_rowf(ui, 15U,
             "  Zakres: %-26s PLL:%3u kHz  BLER:%u/%u  "
             "RSSI:%03u/127 %3u%%  GLOS:%02u/15",
             frequency_range, radio_spacing_khz(&app->settings),
             app->radio.status.bler_a,
             app->radio.status.bler_b, app->radio.status.rssi, rssi_percent,
             app->settings.volume);
  write_rowf(ui, 16U, ANSI_CYAN "%s" ANSI_RESET, border);
  write_rowf(ui, 17U,
             "  " ANSI_SELECTED "[P] STROJ" ANSI_RESET
             "  " ANSI_SELECTED "[O] MENU" ANSI_RESET
             "  " ANSI_SELECTED "[< / >] SEEK" ANSI_RESET
             "  " ANSI_SELECTED "[^ / v] GLOSNOSC" ANSI_RESET
             "  " ANSI_SELECTED "[S] MONO/STEREO" ANSI_RESET
             "  " ANSI_SELECTED "[M] MUTE" ANSI_RESET
             "  P/O ponownie = wyjscie");
  write_rowf(ui, 18U, ANSI_CYAN "%s" ANSI_RESET, border);
  write_rowf(ui, 19U,
             ANSI_CYAN "| USTAWIENIA" ANSI_RESET
             "  %s%-24s%s  %s",
             ui->mode == TERMINAL_UI_MENU_EDIT ? ANSI_YELLOW : ANSI_GREEN,
             mode_name(ui->mode), ANSI_RESET,
             ui->mode == TERMINAL_UI_HOME
                 ? "Nacisnij O, aby aktywowac liste."
                 : (ui->mode == TERMINAL_UI_FREQUENCY
                        ? "Lista nieaktywna podczas edycji czestotliwosci."
                        : "Strzalki wybieraja i zmieniaja opcje."));
  write_rowf(ui, 20U, ANSI_CYAN "%s" ANSI_RESET, border);
  render_menu(ui, app);
  write_rowf(ui, 33U, ANSI_CYAN "%s" ANSI_RESET, border);
  if (ui->mode == TERMINAL_UI_FREQUENCY) {
    write_rowf(ui, 34U,
               "  EDYCJA: " ANSI_YELLOW
               "lewo/prawo = cyfra, gora/dol = krok pozycji, 0-9 = wpisz, "
               "P/Enter/Esc = zakoncz" ANSI_RESET
               "   Limit bezczynnosci: 5 s");
  } else if (ui->mode == TERMINAL_UI_MENU ||
             ui->mode == TERMINAL_UI_MENU_EDIT) {
    write_rowf(ui, 34U,
               "  MENU: gora/dol = wybor, Enter = edycja/zatwierdz, "
               "lewo/prawo = zmiana, O/Esc = powrot, P = strojenie");
  } else {
    write_rowf(ui, 34U,
               "  Gora/dol = glosnosc   Lewo/prawo = seek -/+   "
               "P = strojenie   O = opcje   S = stereo/mono   M = mute   R = redraw");
  }
  write_rowf(ui, 35U,
             ANSI_DIM "  Flash: %s%s%s   Zapis po 3 s bez zmian.   "
             "Terminal: 115200 8N1, ANSI/VT100, UTF-8   "
             "Redraw: tylko zmiany." ANSI_RESET,
             app->settings_dirty ? ANSI_YELLOW : ANSI_GREEN,
             app->settings_dirty ? "OCZEKUJE"
                                 : (app->last_save_ok ? "OK" : "BLAD"),
             ANSI_RESET);
  ui->redraw_all = false;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
  terminal_ui_uart_rx_complete(uart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
  terminal_ui_uart_error(uart);
}

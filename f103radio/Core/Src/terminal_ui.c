#include "terminal_ui.h"

#include "uart_debug.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_RESET       "\x1b[0m"
#define ANSI_CYAN        "\x1b[36m"
#define ANSI_GREEN       "\x1b[32m"
#define ANSI_YELLOW      "\x1b[33m"
#define ANSI_RED         "\x1b[31m"
#define ANSI_DIM         "\x1b[2m"
#define ANSI_SELECTED    "\x1b[30;46m"
#define ANSI_DIGIT       "\x1b[30;43m"
#define TERMINAL_RENDER_INTERVAL_MS 1000U
#define TERMINAL_ESCAPE_TIMEOUT_MS 60U
#define TERMINAL_MENU_ROWS 12

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
  KEY_DIGIT
} key_code_t;

typedef struct {
  key_code_t code;
  uint8_t digit;
} key_event_t;

static terminal_ui_t *active_terminal;

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
    else if (value == 'k' || value == 'K' || value == 'w' || value == 'W') event.code = KEY_UP;
    else if (value == 'j' || value == 'J' || value == 's' || value == 'S') event.code = KEY_DOWN;
    else if (value == 'h' || value == 'H' || value == 'a' || value == 'A' || value == '-') event.code = KEY_LEFT;
    else if (value == 'l' || value == 'L' || value == 'd' || value == 'D' || value == '+') event.code = KEY_RIGHT;
    else if (value == 'r' || value == 'R') event.code = KEY_REFRESH;
    else if (value == 'm' || value == 'M') event.code = KEY_MUTE;
    else if (isdigit((int)value)) { event.code = KEY_DIGIT; event.digit = (uint8_t)(value - '0'); }
  } else if (ui->parser_state == 1U) {
    if (value == '[' || value == 'O') ui->parser_state = 2U;
    else { ui->parser_state = 0U; event.code = KEY_ESCAPE; }
  } else {
    ui->parser_state = 0U;
    if (value == 'A') event.code = KEY_UP;
    else if (value == 'B') event.code = KEY_DOWN;
    else if (value == 'C') event.code = KEY_RIGHT;
    else if (value == 'D') event.code = KEY_LEFT;
    else event.code = KEY_NONE;
  }
  return event;
}

static uint32_t digit_power(uint8_t digit) {
  static const uint32_t powers[6] = {100000U, 10000U, 1000U, 100U, 10U, 1U};
  return powers[digit < 6U ? digit : 5U];
}

static void replace_frequency_digit(terminal_ui_t *ui, radio_app_t *app,
                                    uint8_t digit, uint32_t now_ms) {
  char digits[7];
  uint32_t frequency;
  snprintf(digits, sizeof(digits), "%06lu",
           (unsigned long)app->settings.frequency_khz);
  digits[ui->frequency_digit] = (char)('0' + digit);
  frequency = (uint32_t)strtoul(digits, NULL, 10);
  radio_app_set_frequency(app, frequency, now_ms);
  if (ui->frequency_digit < 5U) ++ui->frequency_digit;
}

static void edit_frequency_arrow(terminal_ui_t *ui, radio_app_t *app,
                                 key_code_t code, uint32_t now_ms) {
  if (code == KEY_LEFT) {
    if (ui->frequency_digit > 0U) --ui->frequency_digit;
  } else if (code == KEY_RIGHT) {
    if (ui->frequency_digit < 5U) ++ui->frequency_digit;
  } else if (code == KEY_UP || code == KEY_DOWN) {
    const int32_t delta = (int32_t)digit_power(ui->frequency_digit) *
                          (code == KEY_UP ? 1 : -1);
    const int32_t candidate = (int32_t)app->settings.frequency_khz + delta;
    radio_app_set_frequency(app, (uint32_t)(candidate < 0 ? 0 : candidate),
                            now_ms);
  }
}

static void handle_key(terminal_ui_t *ui, radio_app_t *app,
                       key_event_t event, uint32_t now_ms) {
  if (event.code == KEY_NONE) return;
  ui->force_render = true;
  if (event.code == KEY_REFRESH) return;
  if (event.code == KEY_MUTE) {
    radio_app_toggle_mute(app, now_ms);
    return;
  }

  if (!ui->editing) {
    if (event.code == KEY_UP) {
      ui->selected = (radio_menu_item_t)((ui->selected + RADIO_MENU_COUNT - 1U) % RADIO_MENU_COUNT);
    } else if (event.code == KEY_DOWN) {
      ui->selected = (radio_menu_item_t)((ui->selected + 1U) % RADIO_MENU_COUNT);
    } else if (event.code == KEY_ENTER || event.code == KEY_RIGHT) {
      if (radio_app_menu_is_action(ui->selected)) {
        radio_app_menu_activate(app, ui->selected, now_ms);
      } else {
        ui->editing = true;
        ui->frequency_digit = 0U;
      }
    }
    return;
  }

  if (event.code == KEY_ENTER || event.code == KEY_ESCAPE) {
    ui->editing = false;
    return;
  }
  if (ui->selected == RADIO_MENU_FREQUENCY) {
    if (event.code == KEY_DIGIT) replace_frequency_digit(ui, app, event.digit, now_ms);
    else edit_frequency_arrow(ui, app, event.code, now_ms);
  } else if (event.code == KEY_LEFT || event.code == KEY_DOWN) {
    radio_app_menu_adjust(app, ui->selected, -1, now_ms);
  } else if (event.code == KEY_RIGHT || event.code == KEY_UP) {
    radio_app_menu_adjust(app, ui->selected, 1, now_ms);
  }
}

static void make_signal_bar(char *output, size_t size, uint8_t rssi) {
  const uint8_t filled = (uint8_t)((rssi * 20U + 126U) / 127U);
  if (size < 23U) return;
  output[0] = '[';
  for (uint8_t i = 0U; i < 20U; ++i) output[i + 1U] = i < filled ? '#' : '.';
  output[21] = ']';
  output[22] = '\0';
}

static void trim_copy(char *output, size_t output_size, const char *input,
                      size_t input_limit) {
  size_t length = strnlen(input, input_limit);
  while (length > 0U && input[length - 1U] == ' ') --length;
  if (length >= output_size) length = output_size - 1U;
  memcpy(output, input, length);
  output[length] = '\0';
}

static void frequency_with_cursor(const terminal_ui_t *ui,
                                  const radio_app_t *app, char *output,
                                  size_t output_size) {
  char digits[7];
  size_t position = 0U;
  snprintf(digits, sizeof(digits), "%06lu",
           (unsigned long)app->settings.frequency_khz);
  for (uint8_t i = 0U; i < 6U && position + 16U < output_size; ++i) {
    if (i == 3U) output[position++] = '.';
    if (ui->editing && ui->selected == RADIO_MENU_FREQUENCY &&
        i == ui->frequency_digit) {
      position += (size_t)snprintf(output + position, output_size - position,
                                   ANSI_DIGIT "%c" ANSI_RESET, digits[i]);
    } else {
      output[position++] = digits[i];
      output[position] = '\0';
    }
  }
  snprintf(output + position, output_size - position, " MHz");
}

void terminal_ui_init(terminal_ui_t *ui, UART_HandleTypeDef *uart,
                      uint32_t now_ms) {
  if (ui == NULL || uart == NULL) return;
  memset(ui, 0, sizeof(*ui));
  ui->uart = uart;
  ui->parser_changed_ms = now_ms;
  ui->force_render = true;
  active_terminal = ui;
  uart_debug_init(uart);
  uart_debug_write("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l\x1b]0;RDA5807 STM32\x07");
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
      (uint32_t)(now_ms - ui->parser_changed_ms) >= TERMINAL_ESCAPE_TIMEOUT_MS) {
    ui->parser_state = 0U;
    handle_key(ui, app, (key_event_t){KEY_ESCAPE, 0U}, now_ms);
  }
}

void terminal_ui_render(terminal_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char ps[16];
  char rt[65];
  char signal[24];
  char frequency[64];
  char value[32];
  int16_t first;
  if (ui == NULL || app == NULL) return;
  if (!ui->force_render &&
      (uint32_t)(now_ms - ui->last_render_ms) < TERMINAL_RENDER_INTERVAL_MS) return;
  ui->force_render = false;
  ui->last_render_ms = now_ms;
  ui->rendered_revision = app->revision;

  trim_copy(ps, sizeof(ps), app->rds.ps_valid ? app->rds.program_service : "--", 8U);
  trim_copy(rt, sizeof(rt), app->rds.radio_text, 64U);
  make_signal_bar(signal, sizeof(signal), app->radio.status.rssi);
  frequency_with_cursor(ui, app, frequency, sizeof(frequency));

  uart_debug_write("\x1b[H");
  uart_debug_printf(ANSI_CYAN "+--------------------------------------------------------------------------------------------------+" ANSI_RESET "\r\n");
  uart_debug_printf(ANSI_CYAN "|" ANSI_RESET "  " ANSI_GREEN "RDA5807M / STM32F103 - odbiornik FM" ANSI_RESET "%59s" ANSI_CYAN "|" ANSI_RESET "\r\n", "");
  uart_debug_printf(ANSI_CYAN "+--------------------------------------------------------------------------------------------------+" ANSI_RESET "\r\n");
  uart_debug_printf("  Częstotliwość: " ANSI_YELLOW "%s" ANSI_RESET "          Głośność: %2u/15   Wyciszenie: %s\r\n",
                    frequency, app->settings.volume, app->settings.muted ? "TAK" : "NIE");
  uart_debug_printf("  Stacja: " ANSI_GREEN "%-8s" ANSI_RESET "  PI: %04X  PTY: %-20s  Tryb: %s\r\n",
                    ps, app->rds.program_id,
                    rds_program_type_name(app->rds.program_type),
                    app->radio.status.stereo && !app->settings.force_mono ? "STEREO" : "MONO");
  uart_debug_printf("  Sygnał: %s %3u/127   FM: %-3s   RDS: %-4s   Tuner: %s\r\n",
                    signal, app->radio.status.rssi,
                    app->radio.status.station_valid ? "TAK" : "NIE",
                    app->radio.status.rds_synchronized ? "SYNC" : "BRAK",
                    app->radio_available ? "ONLINE" : ANSI_RED "OFFLINE" ANSI_RESET);
  uart_debug_printf("  RadioText: %-84.84s\r\n", rt[0] != '\0' ? rt : "--");
  if (app->rds.clock_valid) {
    uart_debug_printf("  Czas RDS: %02u:%02u   MJD: %u   TP:%s TA:%s\r\n",
                      app->rds.local_hour, app->rds.local_minute,
                      app->rds.modified_julian_day,
                      app->rds.traffic_program ? "TAK" : "NIE",
                      app->rds.traffic_announcement ? "TAK" : "NIE");
  } else {
    uart_debug_printf("  Czas RDS: --:--        TP:%s TA:%s\r\n",
                      app->rds.traffic_program ? "TAK" : "NIE",
                      app->rds.traffic_announcement ? "TAK" : "NIE");
  }
  uart_debug_printf(ANSI_CYAN "+-------------------------------- USTAWIENIA --------------------------------+----------------------+" ANSI_RESET "\r\n");

  first = (int16_t)ui->selected - TERMINAL_MENU_ROWS / 2;
  if (first < 0) first = 0;
  if (first > (int16_t)RADIO_MENU_COUNT - TERMINAL_MENU_ROWS)
    first = RADIO_MENU_COUNT - TERMINAL_MENU_ROWS;
  for (uint8_t row = 0U; row < TERMINAL_MENU_ROWS; ++row) {
    const radio_menu_item_t item = (radio_menu_item_t)(first + row);
    radio_app_menu_value(app, item, value, sizeof(value));
    if (item == ui->selected) {
      uart_debug_printf(ANSI_SELECTED "> %02u  %-31s %-30s" ANSI_RESET,
                        (unsigned)item + 1U, radio_app_menu_label(item), value);
      uart_debug_printf("  %s\r\n", ui->editing ? ANSI_YELLOW "EDYCJA" ANSI_RESET : "WYBRANE");
    } else {
      uart_debug_printf("  %02u  %-31s %-30s\r\n",
                        (unsigned)item + 1U, radio_app_menu_label(item), value);
    }
  }
  uart_debug_printf(ANSI_CYAN "+--------------------------------------------------------------------------------------------------+" ANSI_RESET "\r\n");
  uart_debug_printf("  ↑/↓: wybór  Enter: edycja/zatwierdź  ←/→: zmiana  0-9: cyfra częstotliwości  M: mute  R: odśwież\r\n");
  uart_debug_printf("  Flash: %s%s%s   Zapis po 3 s bez zmian.   Operacja tunera: %u\r\n",
                    app->settings_dirty ? ANSI_YELLOW : ANSI_GREEN,
                    app->settings_dirty ? "OCZEKUJE" : (app->last_save_ok ? "OK" : "BLAD"),
                    ANSI_RESET, (unsigned)app->radio.operation);
  uart_debug_write(ANSI_DIM "  Terminal: 115200 8N1, ANSI/VT100, UTF-8" ANSI_RESET "\x1b[J");
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
  terminal_ui_uart_rx_complete(uart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
  terminal_ui_uart_error(uart);
}

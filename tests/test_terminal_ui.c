#include "terminal_ui.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t transmitted_bytes;
static uint32_t clear_sequences;
static uint32_t cursor_sequences;
static uint32_t solid_block_sequences;
static uint32_t hash_characters;

static void inspect_sequences(const uint8_t *data, uint16_t size) {
  for (uint16_t index = 0U; index < size; ++index) {
    if (data[index] == '#') ++hash_characters;
    if (index + 2U < size && data[index] == 0xE2U &&
        data[index + 1U] == 0x96U && data[index + 2U] == 0x88U) {
      ++solid_block_sequences;
    }
  }
  for (uint16_t index = 0U; index + 3U < size; ++index) {
    if (data[index] != 0x1BU || data[index + 1U] != '[') continue;
    if (data[index + 2U] == '2' && data[index + 3U] == 'J') {
      ++clear_sequences;
    }
    for (uint16_t end = (uint16_t)(index + 2U);
         end + 2U < size && end < index + 7U; ++end) {
      if (data[end] == ';' && data[end + 1U] == '1' &&
          data[end + 2U] == 'H') {
        ++cursor_sequences;
        break;
      }
    }
  }
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data,
                                    uint16_t size, uint32_t timeout) {
  (void)uart;
  (void)timeout;
  transmitted_bytes += size;
  inspect_sequences(data, size);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                     uint16_t size) {
  (void)uart;
  (void)data;
  (void)size;
  return HAL_OK;
}

void radio_app_set_frequency(radio_app_t *app, uint32_t frequency_khz,
                             uint32_t now_ms) {
  (void)now_ms;
  app->settings.frequency_khz =
      radio_frequency_clamp(&app->settings, (int32_t)frequency_khz);
}

void radio_app_set_frequency_auto(radio_app_t *app, int32_t frequency_khz,
                                  uint16_t requested_spacing_khz,
                                  uint32_t now_ms) {
  (void)now_ms;
  radio_settings_plan_frequency(&app->settings, frequency_khz,
                                requested_spacing_khz);
}

void radio_app_set_extended_tuning(radio_app_t *app, bool enabled,
                                   uint32_t now_ms) {
  (void)now_ms;
  app->settings.extended_tuning = enabled;
  radio_settings_plan_frequency(&app->settings,
                                (int32_t)app->settings.frequency_khz,
                                radio_spacing_khz(&app->settings));
}

void radio_app_toggle_mute(radio_app_t *app, uint32_t now_ms) {
  (void)now_ms;
  app->settings.muted = !app->settings.muted;
}

void radio_app_seek(radio_app_t *app, bool upwards, uint32_t now_ms) {
  (void)now_ms;
  app->radio.operation = upwards ? RDA5807_OPERATION_SEEKING_UP
                                 : RDA5807_OPERATION_SEEKING_DOWN;
}

const char *radio_app_menu_label(radio_menu_item_t item) {
  static const char *const labels[RADIO_MENU_COUNT] = {
      "Czestotliwosc", "Glosnosc", "Wyciszenie", "Tryb audio",
      "Podbicie basu", "Szukaj w gore", "Szukaj w dol", "Pasmo",
      "Dolne pasmo", "Krok kanalu", "Dekoder RDS", "Standard RDS",
      "Deemfaza", "Koniec szukania", "Alg. szukania", "Prog szukania",
      "Stary prog", "Soft mute", "Soft blend", "Prog soft blend", "AFC",
      "Nowy demodulator", "Wejscie LNA", "Prad LNA", "Kontrast LCD",
      "Negatyw LCD", "Podswietlenie", "Ustawienia domyslne"};
  return item < RADIO_MENU_COUNT ? labels[item] : "?";
}

void radio_app_menu_value(const radio_app_t *app, radio_menu_item_t item,
                          char *buffer, size_t buffer_size) {
  if (item == RADIO_MENU_FREQUENCY) {
    snprintf(buffer, buffer_size, "%lu.%03lu MHz",
             (unsigned long)(app->settings.frequency_khz / 1000U),
             (unsigned long)(app->settings.frequency_khz % 1000U));
  } else {
    snprintf(buffer, buffer_size, "wartosc %u", (unsigned)item);
  }
}

bool radio_app_menu_is_action(radio_menu_item_t item) {
  return item == RADIO_MENU_SEEK_UP || item == RADIO_MENU_SEEK_DOWN ||
         item == RADIO_MENU_DEFAULTS;
}

void radio_app_menu_adjust(radio_app_t *app, radio_menu_item_t item,
                           int16_t delta, uint32_t now_ms) {
  (void)now_ms;
  if (item == RADIO_MENU_VOLUME) {
    int16_t volume = (int16_t)app->settings.volume + delta;
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    app->settings.volume = (uint8_t)volume;
  } else if (item == RADIO_MENU_AUDIO_MODE) {
    app->settings.force_mono = !app->settings.force_mono;
  }
}

void radio_app_menu_activate(radio_app_t *app, radio_menu_item_t item,
                             uint32_t now_ms) {
  if (item == RADIO_MENU_SEEK_UP || item == RADIO_MENU_SEEK_DOWN) {
    radio_app_seek(app, item == RADIO_MENU_SEEK_UP, now_ms);
  }
}

static void queue_key(terminal_ui_t *ui, char key) {
  ui->rx_buffer[ui->rx_head] = (uint8_t)key;
  ui->rx_head =
      (uint8_t)((ui->rx_head + 1U) & (TERMINAL_RX_BUFFER_SIZE - 1U));
}

static void queue_arrow(terminal_ui_t *ui, char direction) {
  queue_key(ui, '\x1b');
  queue_key(ui, '[');
  queue_key(ui, direction);
}

static void prepare_app(radio_app_t *app) {
  memset(app, 0, sizeof(*app));
  radio_settings_defaults(&app->settings);
  app->settings.frequency_khz = 108000U;
  app->settings.volume = 8U;
  app->radio.status.rssi = 73U;
  app->radio.status.stereo = true;
  app->radio.status.station_valid = true;
  app->radio.status.rds_synchronized = true;
  app->radio_available = true;
  app->last_save_ok = true;
  memcpy(app->rds.program_service, "RADIO123", 9U);
  app->rds.ps_valid = true;
  strcpy(app->rds.radio_text, "Przykladowy tekst RDS");
  app->rds.radio_text_valid = true;
}

static void test_incremental_render(void) {
  terminal_ui_t ui;
  radio_app_t app;
  UART_HandleTypeDef uart;
  size_t first_size;
  uint32_t first_cursors;
  uint32_t first_clears;
  memset(&uart, 0, sizeof(uart));
  prepare_app(&app);
  transmitted_bytes = 0U;
  clear_sequences = 0U;
  cursor_sequences = 0U;
  solid_block_sequences = 0U;
  hash_characters = 0U;

  terminal_ui_init(&ui, &uart, 0U);
  terminal_ui_render(&ui, &app, 100U);
  first_size = transmitted_bytes;
  first_cursors = cursor_sequences;
  first_clears = clear_sequences;
  assert(first_size > 0U);
  assert(first_cursors == TERMINAL_SCREEN_ROWS);
  assert(solid_block_sequences > 0U);
  assert(hash_characters == 0U);

  terminal_ui_render(&ui, &app, 200U);
  assert(transmitted_bytes == first_size);
  assert(cursor_sequences == first_cursors);

  app.radio.status.rssi = 100U;
  terminal_ui_render(&ui, &app, 300U);
  assert(transmitted_bytes > first_size);
  assert(cursor_sequences > first_cursors);
  assert(cursor_sequences - first_cursors < TERMINAL_SCREEN_ROWS);
  assert(clear_sequences == first_clears);

  queue_key(&ui, 'R');
  terminal_ui_process(&ui, &app, 400U);
  first_cursors = cursor_sequences;
  terminal_ui_render(&ui, &app, 400U);
  assert(clear_sequences == first_clears + 1U);
  assert(cursor_sequences - first_cursors == TERMINAL_SCREEN_ROWS);
}

static void test_modes_and_shortcuts(void) {
  terminal_ui_t ui;
  radio_app_t app;
  UART_HandleTypeDef uart;
  memset(&uart, 0, sizeof(uart));
  prepare_app(&app);
  terminal_ui_init(&ui, &uart, 0U);

  app.settings.frequency_khz = 106000U;
  queue_key(&ui, 'P');
  terminal_ui_process(&ui, &app, 1000U);
  assert(ui.mode == TERMINAL_UI_FREQUENCY);
  assert(ui.frequency_digit == 3U);
  queue_arrow(&ui, 'B');
  terminal_ui_process(&ui, &app, 1100U);
  assert(app.settings.frequency_khz == 105900U);
  app.settings.frequency_khz = 105950U;
  queue_arrow(&ui, 'A');
  terminal_ui_process(&ui, &app, 1150U);
  assert(app.settings.frequency_khz == 106050U);
  queue_key(&ui, 'P');
  terminal_ui_process(&ui, &app, 1200U);
  assert(ui.mode == TERMINAL_UI_HOME);

  queue_key(&ui, 'P');
  terminal_ui_process(&ui, &app, 1300U);
  terminal_ui_process(&ui, &app, 6301U);
  assert(ui.mode == TERMINAL_UI_HOME);

  queue_key(&ui, 'O');
  terminal_ui_process(&ui, &app, 7000U);
  assert(ui.mode == TERMINAL_UI_MENU);
  queue_key(&ui, 'O');
  terminal_ui_process(&ui, &app, 7050U);
  assert(ui.mode == TERMINAL_UI_HOME);

  queue_arrow(&ui, 'A');
  terminal_ui_process(&ui, &app, 7100U);
  assert(app.settings.volume == 9U);
  queue_arrow(&ui, 'B');
  terminal_ui_process(&ui, &app, 7200U);
  assert(app.settings.volume == 8U);
  queue_arrow(&ui, 'D');
  terminal_ui_process(&ui, &app, 7300U);
  assert(app.radio.operation == RDA5807_OPERATION_SEEKING_DOWN);
  queue_arrow(&ui, 'C');
  terminal_ui_process(&ui, &app, 7400U);
  assert(app.radio.operation == RDA5807_OPERATION_SEEKING_UP);
  assert(!app.settings.force_mono);
  queue_key(&ui, 'S');
  terminal_ui_process(&ui, &app, 7500U);
  assert(app.settings.force_mono);

  queue_key(&ui, 'O');
  terminal_ui_process(&ui, &app, 7600U);
  queue_arrow(&ui, 'B');
  terminal_ui_process(&ui, &app, 7650U);
  assert(ui.selected == (radio_menu_item_t)RADIO_MENU_COUNT);
  queue_key(&ui, '\r');
  terminal_ui_process(&ui, &app, 7675U);
  assert(ui.mode == TERMINAL_UI_MENU_EDIT);
  queue_arrow(&ui, 'C');
  terminal_ui_process(&ui, &app, 7680U);
  assert(app.settings.extended_tuning);
  queue_key(&ui, '\r');
  terminal_ui_process(&ui, &app, 7690U);
  assert(ui.mode == TERMINAL_UI_MENU);
  for (uint8_t index = 0U; index < 7U; ++index) queue_arrow(&ui, 'B');
  terminal_ui_process(&ui, &app, 7700U);
  assert(ui.selected == RADIO_MENU_RDS);

  queue_key(&ui, 'O');
  terminal_ui_process(&ui, &app, 7800U);
  assert(ui.mode == TERMINAL_UI_HOME);
  radio_settings_plan_frequency(&app.settings, RADIO_REGISTER_MAX_KHZ, 50U);
  queue_key(&ui, 'P');
  terminal_ui_process(&ui, &app, 7900U);
  queue_arrow(&ui, 'B');
  terminal_ui_process(&ui, &app, 8000U);
  assert(app.settings.frequency_khz == 291400U);
}

int main(void) {
  test_incremental_render();
  test_modes_and_shortcuts();
  puts("Terminal UI host tests passed.");
  return 0;
}

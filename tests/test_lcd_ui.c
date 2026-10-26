#include "lcd_home.h"
#include "radio_clock.h"
#include "board_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

/* Exercise production UI, framebuffer drawing, radio model and flash storage.
 * Only physical peripherals are replaced. */
static radio_clock_time_t test_clock = {.year = 2026, .month = 10, .day = 26,
                                      .hour = 13, .minute = 47};
static bool clock_auto = true;
static bool clock_works = true;
static bool flash_fails;
static uint32_t tick;
static unsigned radio_writes;

bool radio_clock_read(radio_clock_time_t *time) { *time = test_clock; return clock_works; }
bool radio_clock_set(uint8_t hour, uint8_t minute) {
  if (!clock_works) return false;
  assert(hour < 24U && minute < 60U);
  test_clock.hour = hour; test_clock.minute = minute; clock_auto = false;
  return true;
}
bool radio_clock_rds_sync_enabled(void) { return clock_auto; }
void radio_clock_set_rds_sync(bool enabled) { clock_auto = enabled; }

uint32_t HAL_GetTick(void) { return tick++; }
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, const uint8_t *data,
                                   uint16_t size, uint32_t timeout) {
  (void)spi; (void)data; (void)size; (void)timeout; return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *spi,
                                       const uint8_t *data, uint16_t size) {
  (void)spi; (void)data; (void)size; return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *spi) { (void)spi; return HAL_OK; }

HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *error) {
  (void)error;
  if (flash_fails) return HAL_ERROR;
  assert(erase->NbPages == 1U);
  memset((void *)(uintptr_t)erase->PageAddress, 0xFF, 1024);
  return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t data) {
  (void)type; *(uint16_t *)(uintptr_t)address = (uint16_t)data; return HAL_OK;
}

rda5807_result_t rda5807_init(rda5807_t *radio, I2C_HandleTypeDef *i2c,
                              const radio_settings_t *s) {
  (void)s; memset(radio, 0, sizeof(*radio)); radio->i2c = i2c; radio->online = true;
  return RDA5807_OK;
}
rda5807_result_t rda5807_apply_settings(rda5807_t *radio, const radio_settings_t *s) {
  (void)radio; (void)s; ++radio_writes; return RDA5807_OK;
}
rda5807_result_t rda5807_tune(rda5807_t *radio, const radio_settings_t *s) {
  radio->status.frequency_khz = s->frequency_khz;
  radio->operation = RDA5807_OPERATION_TUNING; ++radio_writes; return RDA5807_OK;
}
rda5807_result_t rda5807_seek(rda5807_t *radio, const radio_settings_t *s, bool up) {
  (void)s; radio->operation = up ? RDA5807_OPERATION_SEEKING_UP : RDA5807_OPERATION_SEEKING_DOWN;
  return RDA5807_OK;
}
rda5807_result_t rda5807_poll(rda5807_t *radio, const radio_settings_t *s) {
  (void)radio; (void)s; return RDA5807_OK;
}
rda5807_result_t rda5807_power_down(rda5807_t *radio) { (void)radio; return RDA5807_OK; }

static void map_memory(uintptr_t address, size_t size) {
  assert(mmap((void *)address, size, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == (void *)address);
}

static void blank_flash(void) {
  memset((void *)(uintptr_t)SETTINGS_FLASH_PAGE_A, 0xFF, 2048);
}

static void write_legacy_record(uint16_t version, uint32_t sequence, const radio_settings_t *s) {
  const uint16_t payload = version == 2U ? 32U : (version == 3U ? 232U : 236U);
  uint8_t *bytes = (uint8_t *)(uintptr_t)SETTINGS_FLASH_PAGE_A;
  const uint32_t magic = 0x52414449U;
  uint32_t crc;
  memset(bytes, 0, 1024);
  memcpy(bytes, &magic, 4); memcpy(bytes + 4, &version, 2);
  memcpy(bytes + 6, &payload, 2); memcpy(bytes + 8, &sequence, 4);
  memcpy(bytes + 12, s, version == 2U ? 32U : 40U);
  if (version != 2U) {
    for (uint8_t i = 0; i < s->station_count; ++i) {
      memcpy(bytes + 12 + 40 + i * 16, &s->stations[i].frequency_khz, 4);
      memcpy(bytes + 12 + 40 + i * 16 + 4, s->stations[i].name, 8);
    }
  }
  crc = settings_store_crc32(bytes, 12U + payload);
  memcpy(bytes + 12 + payload, &crc, 4);
}

static void test_storage_migration(void) {
  radio_settings_t saved, loaded;
  settings_store_t store;
  radio_settings_defaults(&saved);
  saved.volume = 5U; saved.afc_enabled = true; saved.station_count = 2U;
  saved.stations[0].frequency_khz = 97550U; strcpy(saved.stations[0].name, "RADIO123");
  saved.stations[1].frequency_khz = 106100U; strcpy(saved.stations[1].name, "RADIOABC");
  for (uint16_t version = 2U; version <= 4U; ++version) {
    radio_settings_defaults(&saved);
    saved.volume = 5U; saved.afc_enabled = true; saved.station_count = 2U;
    saved.stations[0].frequency_khz = 97550U; strcpy(saved.stations[0].name, "RADIO123");
    saved.stations[1].frequency_khz = 106100U; strcpy(saved.stations[1].name, "RADIOABC");
    blank_flash(); settings_store_init(&store);
    write_legacy_record(version, 17U, &saved);
    assert(settings_store_load(&store, &loaded));
    assert(store.sequence == 17U && loaded.volume == 5U && loaded.afc_enabled);
    if (version == 2U) assert(loaded.station_count == 0U);
    else {
      assert(loaded.station_count == 2U);
      assert(loaded.stations[0].frequency_khz == 97550U);
      assert(strcmp(loaded.stations[0].name, "RADIO123") == 0);
      assert(strcmp(loaded.stations[1].name, "RADIOABC") == 0);
    }
    strcpy(loaded.stations[0].name, "RADIO NAZWA DLUGA DO PRZEWIJANIA");
    loaded.station_count = 2U;
    assert(settings_store_save(&store, &loaded));
    assert(store.sequence == 18U);
    assert(settings_store_load(&store, &saved));
    assert(strcmp(saved.stations[0].name, "RADIO NAZWA DLUGA DO PRZEWIJANIA") == 0);
    flash_fails = true;
    assert(!settings_store_save(&store, &saved));
    flash_fails = false;
    assert(settings_store_load(&store, &loaded));
    assert(store.sequence == 18U);
    /* Recreate a short legacy fixture for the next version. */
    strcpy(saved.stations[0].name, "RADIO123");
  }
  blank_flash();
}

static void event(lcd_ui_t *ui, radio_app_t *app, input_event_t e) {
  tick += 100;
  lcd_ui_handle_input(ui, app, e, tick);
}
static void rotate(lcd_ui_t *ui, radio_app_t *app, int16_t delta) {
  event(ui, app, (input_event_t){.rotation = delta});
}
static void ok(lcd_ui_t *ui, radio_app_t *app) {
  event(ui, app, (input_event_t){.ok = true});
}
static void open_entry(lcd_ui_t *ui, radio_app_t *app, radio_menu_item_t item) {
  for (uint8_t c = 0; c < lcd_category_count; ++c)
    for (uint8_t e = 0; e < lcd_categories[c].entry_count; ++e)
      if (lcd_categories[c].entries[e].item == item) {
        ui->screen = LCD_UI_ITEMS; ui->category = c; ui->selected = e + 1U;
        ok(ui, app); return;
      }
  assert(!"entry missing");
}
static void dump_frame(const char *path, const pcd8544_t *lcd) {
  FILE *file = fopen(path, "wb"); assert(file != NULL);
  assert(fwrite(lcd->buffer, 1, sizeof(lcd->buffer), file) == sizeof(lcd->buffer));
  assert(fclose(file) == 0);
}

static bool pixel(const pcd8544_t *lcd, uint8_t x, uint8_t y) {
  return (lcd->buffer[(y / 8U) * 84U + x] & (1U << (y % 8U))) != 0;
}
static void assert_graphic(const pcd8544_t *lcd, lcd_region_id_t id, uint8_t variant) {
  const lcd_region_t *r = &lcd_regions[id];
  const lcd_asset_t *a = r->graphics[variant];
  assert(r->width == a->width && r->height == a->height);
  for (uint8_t y = 0; y < r->height; ++y)
    for (uint8_t x = 0; x < r->width; ++x) {
      const bool expected = (a->frames[0].pixels[(y / 8U) * a->width + x] & (1U << (y % 8U))) != 0;
      assert(pixel(lcd, r->x + x, r->y + y) == expected);
    }
}

static void test_home(lcd_ui_t *ui, radio_app_t *app) {
  pcd8544_t *lcd = ui->lcd;
  app->radio.operation = RDA5807_OPERATION_IDLE;
  app->settings.volume = 15; app->settings.bass_boost = true;
  app->radio.status = (rda5807_status_t){.rssi = 127, .stereo = true,
                                      .station_valid = true, .rds_synchronized = true};
  app->rds.ps_valid = true; strcpy(app->rds.program_service, "RADIO123");
  app->rds.program_type = 11; app->rds.radio_text_valid = true;
  strcpy(app->rds.radio_text, "ARTYSTA I TYTUL UTWORU PRZEWIJANY Z ODSTEPEM");
  assert(radio_app_station_add(app, app->settings.frequency_khz, "RADIO ULUBIONA STACJA Z PAMIECI", tick));
  lcd_home_render(ui, app, 100U);
  assert_graphic(lcd, LCD_REGION_SIGNAL, 8);
  assert_graphic(lcd, LCD_REGION_VOLUME, 8);
  assert_graphic(lcd, LCD_REGION_SIGNAL_ICON, 0);
  assert_graphic(lcd, LCD_REGION_SPEAKER, 0);
  assert_graphic(lcd, LCD_REGION_BASS, 0);
  assert_graphic(lcd, LCD_REGION_STEREO, 0);
  assert_graphic(lcd, LCD_REGION_RDS, 0);
  assert_graphic(lcd, LCD_REGION_CONTROL, 1);
  for (uint8_t x = 6; x < 78; ++x) {
    assert(!pixel(lcd, x, 35)); assert(!pixel(lcd, x, 43));
  }
  dump_frame("build/tests/home-rds.bin", lcd);
  lcd_home_render(ui, app, 1900U);
  dump_frame("build/tests/home-scroll.bin", lcd);
  lcd_home_render(ui, app, 100U + (uint32_t)strlen(app->rds.radio_text) * 300U);
  dump_frame("build/tests/home-loop-gap.bin", lcd);
  lcd_home_render(ui, app, 100U + ((uint32_t)strlen(app->rds.radio_text) + 5U) * 300U);
  dump_frame("build/tests/home-loop-restart.bin", lcd);
  app->settings.frequency_khz = 100000U;
  lcd_home_render(ui, app, 18000U);
  dump_frame("build/tests/home-integer-frequency.bin", lcd);
  dump_frame("build/tests/home-short-name.bin", lcd);
  app->settings.frequency_khz = 97550U;
  lcd_home_render(ui, app, 18100U);
  dump_frame("build/tests/home-fraction-frequency.bin", lcd);
  app->settings.frequency_khz = 106100U;
  app->settings.muted = true; app->settings.bass_boost = false;
  app->radio.status.rds_synchronized = false;
  app->radio.status.stereo = false; app->radio.status.station_valid = false;
  lcd_home_render(ui, app, 2000U);
  assert_graphic(lcd, LCD_REGION_SIGNAL, 7);
  assert_graphic(lcd, LCD_REGION_BASS, 1);
  assert_graphic(lcd, LCD_REGION_STEREO, 1);
  assert_graphic(lcd, LCD_REGION_SIGNAL_ICON, 1);
  assert_graphic(lcd, LCD_REGION_SPEAKER, 1);
  assert_graphic(lcd, LCD_REGION_VOLUME, 8); /* Mute preserves the set volume. */
  dump_frame("build/tests/home-logo.bin", lcd);
  app->settings.force_mono = true; app->radio.status.stereo = true;
  app->radio.status.rds_synchronized = true;
  lcd_home_render(ui, app, 2050U);
  assert_graphic(lcd, LCD_REGION_STEREO, 1);
  assert_graphic(lcd, LCD_REGION_SIGNAL, 7);
  app->settings.volume = 0; app->settings.muted = false;
  lcd_home_render(ui, app, 2100U);
  assert_graphic(lcd, LCD_REGION_VOLUME, 0);
  assert_graphic(lcd, LCD_REGION_SPEAKER, 1);
  for (uint8_t raw = 0; raw < 127; ++raw)
    assert(lcd_home_signal_level(raw, true, true) <= lcd_home_signal_level(raw + 1U, true, true));
  assert(lcd_home_signal_level(0, true, true) == 0);
  assert(lcd_home_signal_level(127, false, true) == 7);
  assert(lcd_home_signal_level(127, true, false) == 7);
  radio_settings_defaults(&app->settings);
}

static void test_menu(lcd_ui_t *ui, radio_app_t *app) {
  assert(lcd_category_count == 8U && lcd_visible_rows == 5U);
  event(ui, app, (input_event_t){.click = true});
  assert(ui->screen == LCD_UI_CATEGORIES);
  lcd_ui_render(ui, app, tick + 100);
  dump_frame("build/tests/menu-root.bin", ui->lcd);
  event(ui, app, (input_event_t){.right = true});
  ok(ui, app); assert(ui->screen == LCD_UI_ITEMS && ui->category == 1);
  event(ui, app, (input_event_t){.left = true}); /* Wstecz wraps to last item. */
  ok(ui, app); assert(ui->edited_item == RADIO_MENU_BASS && ui->screen == LCD_UI_EDIT);
  rotate(ui, app, 1); assert(app->settings.bass_boost);
  ok(ui, app);

  open_entry(ui, app, RADIO_MENU_FREQUENCY);
  rotate(ui, app, 1); assert(app->settings.frequency_khz == 106150U);
  rotate(ui, app, -32768); assert(app->settings.frequency_khz == 50000U);
  rotate(ui, app, 32767); assert(app->settings.frequency_khz == 115000U);
  open_entry(ui, app, RADIO_MENU_LCD_CONTRAST);
  rotate(ui, app, -32768); assert(app->settings.lcd_contrast == 20U);
  rotate(ui, app, 32767); assert(app->settings.lcd_contrast == 127U);
  open_entry(ui, app, RADIO_MENU_VOLUME);
  rotate(ui, app, 32767); assert(app->settings.volume == 15U);
  lcd_ui_render(ui, app, tick + 100);
  dump_frame("build/tests/menu-volume.bin", ui->lcd);
  open_entry(ui, app, RADIO_MENU_RSSI_AVERAGE);
  const unsigned before = radio_writes;
  rotate(ui, app, 1); assert(app->settings.rssi_average_ms == 2000U);
  rotate(ui, app, 1); assert(app->settings.rssi_average_ms == 5000U);
  rotate(ui, app, 1); assert(app->settings.rssi_average_ms == 200U);
  assert(radio_writes == before);
  open_entry(ui, app, RADIO_MENU_FIRMWARE_VERSION);
  const radio_settings_t unchanged = app->settings;
  rotate(ui, app, 1); assert(memcmp(&unchanged, &app->settings, sizeof(unchanged)) == 0);
  open_entry(ui, app, RADIO_MENU_DEFAULTS);
  ok(ui, app); assert(app->settings.volume == 15U); /* Default selection is NO. */
  open_entry(ui, app, RADIO_MENU_DEFAULTS);
  rotate(ui, app, 1); ok(ui, app);
  assert(app->settings.volume == 8U && !app->settings.afc_enabled);
  assert(app->settings.frequency_khz == 106100U && app->settings.station_count == 0U);

  open_entry(ui, app, RADIO_MENU_CLOCK);
  rotate(ui, app, 11); assert(ui->clock_hour == 0U);
  ok(ui, app); rotate(ui, app, 13); assert(ui->clock_minute == 0U);
  clock_works = false; ok(ui, app); assert(ui->screen == LCD_UI_CLOCK && ui->clock_error);
  clock_works = true; ok(ui, app);
  assert(ui->screen == LCD_UI_ITEMS && !clock_auto && test_clock.hour == 0 && test_clock.minute == 0);
  open_entry(ui, app, RADIO_MENU_CLOCK_RDS); rotate(ui, app, 1); assert(clock_auto);

  app->rds.ps_valid = true; open_entry(ui, app, RADIO_MENU_RDS);
  rotate(ui, app, 1); assert(!app->settings.rds_enabled && !app->rds.ps_valid);
  rotate(ui, app, 1); assert(app->settings.rds_enabled);
  open_entry(ui, app, RADIO_MENU_SEEK_UP);
  assert(ui->screen == LCD_UI_ACTION && app->radio.operation == RDA5807_OPERATION_SEEKING_UP);
}

static void test_station_wizard(lcd_ui_t *ui, radio_app_t *app) {
  open_entry(ui, app, RADIO_MENU_ADD_STATION);
  assert(ui->screen == LCD_UI_STATION_FREQUENCY);
  rotate(ui, app, 1); ok(ui, app);
  assert(ui->screen == LCD_UI_STATION_NAME && !ui->name_editing);
  ok(ui, app); rotate(ui, app, 1); ok(ui, app); /* S -> T */
  assert(ui->station_name[0] == 'T' && ui->name_cursor == 1U);
  rotate(ui, app, 6); ok(ui, app); /* Select Save. */
  assert(app->settings.station_count == 1U && strcmp(app->settings.stations[0].name, "TTACJA") == 0);
  assert(app->settings.stations[0].frequency_khz == 106150U);
  assert(ui->screen == LCD_UI_STATION_ACTIONS);
  assert(radio_app_flush_settings(app));
  radio_app_t rebooted; radio_app_init(&rebooted, NULL, tick);
  assert(rebooted.settings.station_count == 1U && strcmp(rebooted.settings.stations[0].name, "TTACJA") == 0);
  assert(radio_app_station_update(app, 0, 97550, "RADIO BARDZO DLUGA NAZWA STACJI", tick));
  ui->selected = 3U; ok(ui, app);
  lcd_ui_render(ui, app, tick + 100);
  dump_frame("build/tests/station-name.bin", ui->lcd);
  rotate(ui, app, 32); /* 31-character name plus one append position. */
  ok(ui, app);
  assert(ui->screen == LCD_UI_STATION_ACTIONS);
  ui->selected = 4U; ok(ui, app);
  assert(ui->screen == LCD_UI_STATION_DELETE);
  ok(ui, app); assert(app->settings.station_count == 1U);
  ui->selected = 4U; ok(ui, app); rotate(ui, app, 1); ok(ui, app);
  assert(app->settings.station_count == 0U);
}

int main(void) {
  map_memory(0x0800F000U, 4096);
  map_memory(0x40010000U, 8192);
  test_storage_migration();
  radio_app_t app;
  pcd8544_t lcd = {0};
  lcd_ui_t ui;
  radio_app_init(&app, NULL, 0);
  lcd_ui_init(&ui, &lcd, &app.settings);
  test_home(&ui, &app);
  test_menu(&ui, &app);
  test_station_wizard(&ui, &app);
  puts("LCD UI, radio actions and settings migration tests passed.");
  return 0;
}

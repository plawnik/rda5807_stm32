#include "lcd_home.h"

#include "radio_clock.h"

#include <stdio.h>
#include <string.h>

#define SCROLL_INTERVAL_MS 300U
#define SCROLL_GAP 5U

static uint8_t advance(const lcd_font_t *font, char c) {
  /* A one-pixel colon/dot lets HH:MM fit the specified 18-pixel field. */
  if (font == &lcd_tiny_font && (c == ':' || c == '.')) return 2U;
  return (uint8_t)(font->width + font->spacing);
}

void lcd_draw_text(pcd8544_t *lcd, const lcd_font_t *font, int16_t x,
                   int16_t y, uint8_t width, const char *text,
                   lcd_align_t align, bool scroll, uint32_t elapsed_ms) {
  const size_t length = strlen(text);
  uint16_t pixels = 0U;
  int16_t cursor = x;
  size_t first = 0U;
  size_t count = length;
  const uint8_t normal_advance = (uint8_t)(font->width + font->spacing);
  if (length == 0U || width == 0U) return;
  for (size_t i = 0U; i < length; ++i) pixels += advance(font, text[i]);
  pixels -= font->spacing;
  if (pixels <= width) {
    if (align == LCD_ALIGN_CENTER) cursor += (int16_t)((width - pixels) / 2U);
    else if (align == LCD_ALIGN_RIGHT) cursor += (int16_t)(width - pixels);
  } else if (scroll) {
    first = (elapsed_ms / SCROLL_INTERVAL_MS) % (length + SCROLL_GAP);
    count = width / normal_advance + 2U;
  }
  for (size_t i = 0U; i < count && cursor < x + width; ++i) {
    const size_t position = (first + i) % (length + SCROLL_GAP);
    char c = position < length ? text[position] : ' ';
    uint8_t glyph;
    uint8_t shift = 0U;
    if ((uint8_t)c < 32U || (uint8_t)c > 127U) c = '?';
    glyph = (uint8_t)c - 32U;
    if (font == &lcd_tiny_font && (c == ':' || c == '.')) shift = 1U;
    for (uint8_t col = shift; col < font->width; ++col) {
      const int16_t xx = (int16_t)(cursor + col - shift);
      if (xx < x || xx >= x + width) continue;
      for (uint8_t row = 0U; row < font->height; ++row) {
        if ((font->glyphs[glyph * font->width + col] & (1U << row)) != 0U)
          pcd8544_set_pixel(lcd, xx, (int16_t)(y + row), true);
      }
    }
    cursor += advance(font, c);
  }
}

static void region_text(pcd8544_t *lcd, lcd_region_id_t id,
                         const lcd_font_t *font, const char *text,
                         lcd_align_t align, bool scroll, uint32_t elapsed_ms) {
  const lcd_region_t *r = &lcd_regions[id];
  lcd_draw_text(lcd, font, r->x, r->y, r->width, text, align, scroll, elapsed_ms);
}

static void region_graphic(pcd8544_t *lcd, lcd_region_id_t id,
                            uint8_t variant, uint32_t elapsed_ms) {
  const lcd_region_t *r = &lcd_regions[id];
  const lcd_asset_t *asset;
  const lcd_frame_t *frame;
  uint32_t duration = 0U;
  uint32_t time;
  uint16_t index = 0U;
  if (variant >= r->graphic_count) return;
  asset = r->graphics[variant];
  for (uint16_t i = 0U; i < asset->frame_count; ++i)
    duration += asset->frames[i].duration_ms;
  if (duration == 0U) return;
  time = asset->loop ? elapsed_ms % duration
                      : (elapsed_ms < duration ? elapsed_ms : duration - 1U);
  while (index + 1U < asset->frame_count &&
         time >= asset->frames[index].duration_ms) {
    time -= asset->frames[index++].duration_ms;
  }
  frame = &asset->frames[index];
  for (uint8_t y = 0U; y < r->height; ++y) {
    const uint8_t sy = (uint8_t)((uint16_t)y * asset->height / r->height);
    for (uint8_t x = 0U; x < r->width; ++x) {
      const uint8_t sx = (uint8_t)((uint16_t)x * asset->width / r->width);
      if ((frame->pixels[(sy / 8U) * asset->width + sx] & (1U << (sy % 8U))) != 0U)
        pcd8544_set_pixel(lcd, (int16_t)(r->x + x), (int16_t)(r->y + y), true);
    }
  }
}

uint8_t lcd_home_signal_level(uint8_t rssi, bool stereo, bool rds) {
  /* Empirical raw RSSI codes, NOT dBm. Calibrate against the actual antenna.
   * Level 8 additionally qualifies the stereo decoder and RDS synchronization. */
  static const uint8_t thresholds[8] = {8, 16, 24, 32, 40, 50, 60, 70};
  uint8_t level = 0U;
  while (level < 8U && rssi >= thresholds[level]) ++level;
  if (level == 8U && (!stereo || !rds)) level = 7U;
  return level;
}

static const char *short_program_type(uint8_t type, bool rbds) {
  static const char *const rds_names[32] = {
      "", "INFO", "PUB", "INFO", "SPORT", "EDUK", "TEATR", "KULT",
      "NAUKA", "ROZR", "POP", "ROCK", "LEKKA", "KLAS", "KLAS", "INNA",
      "POGODA", "GOSP", "DZIECI", "SPOL", "RELIGIA", "LIVE", "PODR", "HOBBY",
      "JAZZ", "INNA", "INNA", "HITY", "FOLK", "DOK", "TEST", "ALARM"};
  static const char *const rbds_names[32] = {
      "", "INFO", "INFO", "INFO", "SPORT", "TALK", "ROCK", "ROCK", "HITY",
      "HITY", "COUNTRY", "HITY", "LEKKA", "OLDIES", "LEKKA", "KLAS", "R&B",
      "R&B", "JEZYKI", "RELIGIA", "RELIGIA", "OSOBOW", "PUB", "EDUK",
      "", "", "", "", "", "POGODA", "TEST", "ALARM"};
  return rbds ? rbds_names[type & 31U] : rds_names[type & 31U];
}

static void draw_logo(pcd8544_t *lcd, uint8_t variant) {
  const lcd_region_t *r = &lcd_regions[LCD_REGION_LOGO];
  const int16_t x = r->x;
  const int16_t y = r->y;
  if ((variant & 1U) == 0U) {
    pcd8544_rect(lcd, x + 14, y + 7, 44, 17, true);
    pcd8544_line(lcd, x + 18, y + 6, x + 32, y + 2, true);
    pcd8544_line(lcd, x + 32, y + 2, x + 47, y + 2, true);
    pcd8544_rect(lcd, x + 18, y + 11, 13, 9, true);
    pcd8544_line(lcd, x + 20, y + 15, x + 24, y + 12, true);
    pcd8544_line(lcd, x + 24, y + 12, x + 29, y + 17, true);
    pcd8544_line(lcd, x + 29, y + 17, x + 25, y + 20, true);
    pcd8544_line(lcd, x + 25, y + 20, x + 20, y + 15, true);
    pcd8544_line(lcd, x + 36, y + 12, x + 52, y + 12, true);
    pcd8544_fill_rect(lcd, x + 37, y + 17, 3, 4, true);
    pcd8544_fill_rect(lcd, x + 43, y + 15, 3, 6, true);
    pcd8544_fill_rect(lcd, x + 49, y + 13, 3, 8, true);
  } else {
    pcd8544_line(lcd, x + 20, y + 23, x + 28, y + 2, true);
    pcd8544_line(lcd, x + 28, y + 2, x + 36, y + 23, true);
    pcd8544_line(lcd, x + 23, y + 15, x + 33, y + 15, true);
    pcd8544_fill_rect(lcd, x + 27, y + 18, 3, 7, true);
    pcd8544_line(lcd, x + 17, y + 7, x + 22, y + 11, true);
    pcd8544_line(lcd, x + 17, y + 19, x + 22, y + 15, true);
    pcd8544_line(lcd, x + 39, y + 11, x + 44, y + 7, true);
    pcd8544_line(lcd, x + 39, y + 15, x + 44, y + 19, true);
    lcd_draw_text(lcd, &lcd_menu_font, x + 47, y + 12, 18, "FM",
                   LCD_ALIGN_CENTER, false, 0U);
  }
}

static uint32_t text_key(const radio_app_t *app, const radio_station_t *station) {
  uint32_t key = app->settings.frequency_khz;
  for (const char *p = app->rds.radio_text; *p != '\0'; ++p) key = key * 33U + (uint8_t)*p;
  if (station != NULL)
    for (const char *p = station->name; *p != '\0'; ++p) key = key * 33U + (uint8_t)*p;
  return key;
}

void lcd_home_render(lcd_ui_t *ui, const radio_app_t *app, uint32_t now_ms) {
  pcd8544_t *lcd = ui->lcd;
  const radio_settings_t *s = &app->settings;
  const rda5807_status_t *status = &app->radio.status;
  const radio_station_t *station = radio_app_current_station(app);
  const uint8_t rssi = radio_app_display_rssi(app);
  const bool stereo = status->stereo && !s->force_mono;
  const bool has_rds = s->rds_enabled && status->rds_synchronized &&
                       app->radio.operation == RDA5807_OPERATION_IDLE;
  const uint32_t key = text_key(app, station);
  uint32_t elapsed;
  char text[32];
  radio_clock_time_t clock;
  memcpy(lcd->buffer, lcd_home_background, sizeof(lcd->buffer));
  /* Erase the editor's placeholder digits before composing dynamic fields. */
  for (uint8_t id = 0U; id < LCD_REGION_COUNT; ++id) {
    const lcd_region_t *r = &lcd_regions[id];
    pcd8544_fill_rect(lcd, r->x, r->y, r->width, r->height, false);
  }
  if (!app->radio_available) {
    lcd_draw_text(lcd, &lcd_menu_font, 6, 12, 72, "BRAK RADIA", LCD_ALIGN_CENTER, false, 0U);
    lcd_draw_text(lcd, &lcd_tiny_font, 6, 24, 72, "SPRAWDZ I2C", LCD_ALIGN_CENTER, false, 0U);
    return;
  }
  if (key != ui->home_text_key) {
    ui->home_text_key = key;
    ui->home_text_started_ms = now_ms;
  }
  elapsed = now_ms - ui->home_text_started_ms;
  region_graphic(lcd, LCD_REGION_SIGNAL, lcd_home_signal_level(rssi, stereo, has_rds), now_ms);
  region_graphic(lcd, LCD_REGION_VOLUME, (uint8_t)(((uint16_t)s->volume * 8U + 7U) / 15U), now_ms);
  region_graphic(lcd, LCD_REGION_SIGNAL_ICON, status->station_valid ? 0U : 1U, now_ms);
  region_graphic(lcd, LCD_REGION_SPEAKER, s->muted || s->volume == 0U ? 1U : 0U, now_ms);
  if (has_rds) region_graphic(lcd, LCD_REGION_RDS, 0U, now_ms);
  region_graphic(lcd, LCD_REGION_BASS, s->bass_boost ? 0U : 1U, now_ms);
  region_graphic(lcd, LCD_REGION_STEREO, stereo ? 0U : 1U, now_ms);
  region_graphic(lcd, LCD_REGION_CONTROL,
                 ui->home_action == RADIO_CONTROL_SEEK ? 0U :
                 (ui->home_action == RADIO_CONTROL_STATIONS ? 2U : 1U), now_ms);
  snprintf(text, sizeof(text), "%02u", (unsigned)(((uint16_t)rssi * 99U + 63U) / 127U));
  region_text(lcd, LCD_REGION_RSSI_NUMBER, &lcd_tiny_font, text, LCD_ALIGN_CENTER, false, 0U);
  snprintf(text, sizeof(text), "%u", s->volume);
  region_text(lcd, LCD_REGION_VOLUME_NUMBER, &lcd_tiny_font, text, LCD_ALIGN_CENTER, false, 0U);
  if (radio_clock_read(&clock)) snprintf(text, sizeof(text), "%02u:%02u", clock.hour, clock.minute);
  else strcpy(text, "--:--");
  region_text(lcd, LCD_REGION_CLOCK, &lcd_tiny_font, text, LCD_ALIGN_CENTER, false, 0U);
  region_text(lcd, LCD_REGION_STATION_NAME, &lcd_tiny_font, station != NULL ? station->name : "FM",
               LCD_ALIGN_CENTER, true, elapsed);
  {
    const uint32_t hundredths = (s->frequency_khz + 5U) / 10U;
    const unsigned fraction = hundredths % 100U;
    if (fraction == 0U) snprintf(text, sizeof(text), "%luMHz", (unsigned long)(hundredths / 100U));
    else if (fraction % 10U == 0U)
      snprintf(text, sizeof(text), "%lu.%uMHz", (unsigned long)(hundredths / 100U), fraction / 10U);
    else snprintf(text, sizeof(text), "%lu.%02uMHz", (unsigned long)(hundredths / 100U), fraction);
  }
  region_text(lcd, LCD_REGION_FREQUENCY, &lcd_menu_font, text, LCD_ALIGN_CENTER, false, 0U);
  if (has_rds) {
    if (app->rds.ps_valid)
      region_text(lcd, LCD_REGION_PS, &lcd_tiny_font, app->rds.program_service, LCD_ALIGN_LEFT, false, 0U);
    region_text(lcd, LCD_REGION_PTY, &lcd_tiny_font,
                 short_program_type(app->rds.program_type, s->rbds_enabled), LCD_ALIGN_RIGHT, false, 0U);
    if (app->rds.radio_text_valid) {
      region_text(lcd, LCD_REGION_RT, &lcd_tiny_font, app->rds.radio_text, LCD_ALIGN_LEFT, false, 0U);
      region_text(lcd, LCD_REGION_RT_SCROLL, &lcd_tiny_font, app->rds.radio_text,
                   LCD_ALIGN_LEFT, true, elapsed);
    }
  } else {
    const uint32_t slot = now_ms / 30000U;
    if (ui->logo_slot != slot) {
      ui->logo_variant ^= 1U;
      ui->logo_slot = slot;
    }
    draw_logo(lcd, ui->logo_variant);
  }
}

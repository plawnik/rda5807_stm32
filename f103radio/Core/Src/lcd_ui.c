#include "lcd_ui.h"

#include <stdio.h>
#include <string.h>

#define LCD_RENDER_INTERVAL_MS 100U
#define LCD_SCROLL_INTERVAL_MS 300U

static size_t visible_length(const char *text, size_t maximum) {
  size_t length = strnlen(text, maximum);
  while (length > 0U && text[length - 1U] == ' ') --length;
  return length;
}

static void scrolling_text(char *output, size_t output_size,
                           const char *input, size_t input_limit,
                           size_t width, uint32_t now_ms) {
  const size_t length = visible_length(input, input_limit);
  if (output_size == 0U) return;
  if (length <= width) {
    const size_t copy = length < output_size - 1U ? length : output_size - 1U;
    memcpy(output, input, copy);
    output[copy] = '\0';
    return;
  }

  const size_t cycle = length + 3U;
  const size_t offset = (now_ms / LCD_SCROLL_INTERVAL_MS) % cycle;
  const size_t count = width < output_size - 1U ? width : output_size - 1U;
  for (size_t i = 0U; i < count; ++i) {
    const size_t position = (offset + i) % cycle;
    output[i] = position < length ? input[position] : ' ';
  }
  output[count] = '\0';
}

static void draw_signal_icon(pcd8544_t *lcd, int16_t x, int16_t y,
                             uint8_t rssi) {
  const uint8_t bars = (uint8_t)((rssi + 25U) / 26U);
  for (uint8_t i = 0U; i < 5U; ++i) {
    const int16_t height = (int16_t)(2 + i);
    pcd8544_rect(lcd, (int16_t)(x + i * 3), (int16_t)(y + 7 - height),
                 2, height, true);
    if (i < bars) {
      pcd8544_fill_rect(lcd, (int16_t)(x + i * 3),
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
    for (uint8_t i = 0U; i < waves; ++i) {
      pcd8544_line(lcd, (int16_t)(x + 8 + i * 2),
                   (int16_t)(y + 2 - i), (int16_t)(x + 8 + i * 2),
                   (int16_t)(y + 5 + i), true);
    }
  }
}

static void draw_menu_icon(pcd8544_t *lcd, radio_menu_item_t item,
                           int16_t y) {
  const int16_t cy = (int16_t)(y + 3);
  if (item == RADIO_MENU_VOLUME || item == RADIO_MENU_MUTE ||
      item == RADIO_MENU_AUDIO_MODE || item == RADIO_MENU_BASS) {
    pcd8544_fill_rect(lcd, 0, (int16_t)(y + 2), 2, 4, true);
    pcd8544_line(lcd, 2, (int16_t)(y + 2), 6, y, true);
    pcd8544_line(lcd, 2, (int16_t)(y + 5), 6, (int16_t)(y + 7), true);
  } else if (item == RADIO_MENU_SEEK_UP || item == RADIO_MENU_SEEK_DOWN) {
    const bool up = item == RADIO_MENU_SEEK_UP;
    pcd8544_line(lcd, 1, cy, 6, cy, true);
    pcd8544_line(lcd, up ? 4 : 3, (int16_t)(cy - 2), up ? 6 : 1, cy, true);
    pcd8544_line(lcd, up ? 4 : 3, (int16_t)(cy + 2), up ? 6 : 1, cy, true);
  } else if (item >= RADIO_MENU_RDS && item <= RADIO_MENU_NEW_METHOD) {
    pcd8544_rect(lcd, 0, y, 7, 7, true);
    pcd8544_char(lcd, 1, y, 'R', 1U, true);
  } else if (item >= RADIO_MENU_LCD_CONTRAST &&
             item <= RADIO_MENU_LCD_BACKLIGHT) {
    pcd8544_rect(lcd, 0, y, 7, 7, true);
    pcd8544_set_pixel(lcd, 3, (int16_t)(y + 3), true);
  } else {
    pcd8544_rect(lcd, 1, (int16_t)(y + 1), 5, 5, true);
  }
}

static void render_home(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char line[20];
  char source[66];
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

  if (rds->ps_valid) memcpy(source, rds->program_service, 9U);
  else strcpy(source, "FM RADIO");
  title_length = visible_length(source, 8U);
  x = (int16_t)((PCD8544_WIDTH - title_length * 6U) / 2U);
  pcd8544_text(ui->lcd, x, 0, source, 1U, true);
  if (status->rds_synchronized) pcd8544_text(ui->lcd, 0, 0, "R", 1U, true);
  if (app->radio.operation != RDA5807_OPERATION_IDLE) {
    pcd8544_text(ui->lcd, 78, 0, ">", 1U, true);
  }

  snprintf(line, sizeof(line), "%lu.%01lu",
           (unsigned long)(app->settings.frequency_khz / 1000U),
           (unsigned long)((app->settings.frequency_khz % 1000U) / 100U));
  x = (int16_t)((PCD8544_WIDTH - strlen(line) * 12U) / 2U);
  pcd8544_text(ui->lcd, x, 9, line, 2U, true);

  draw_volume_icon(ui->lcd, 1, 26, app->settings.volume,
                   app->settings.muted);
  draw_signal_icon(ui->lcd, 26, 26, status->rssi);
  snprintf(line, sizeof(line), "%c %03u",
           status->stereo && !app->settings.force_mono ? 'S' : 'M',
           status->rssi);
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

static void render_menu(lcd_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms) {
  char header[16];
  int16_t first = (int16_t)ui->selected - 2;
  if (first < 0) first = 0;
  if (first > (int16_t)RADIO_MENU_COUNT - 5) first = RADIO_MENU_COUNT - 5;

  snprintf(header, sizeof(header), "MENU %02u/%02u%c",
           (unsigned)ui->selected + 1U, (unsigned)RADIO_MENU_COUNT,
           ui->editing ? '*' : ' ');
  pcd8544_text(ui->lcd, 1, 0, header, 1U, true);
  pcd8544_invert_rect(ui->lcd, 0, 0, PCD8544_WIDTH, 8);

  for (uint8_t row = 0U; row < 5U; ++row) {
    const radio_menu_item_t item = (radio_menu_item_t)(first + row);
    const int16_t y = (int16_t)(8 + row * 8);
    char combined[64];
    char value[24];
    char shown[16];
    draw_menu_icon(ui->lcd, item, y);
    if (item == ui->selected) {
      radio_app_menu_value(app, item, value, sizeof(value));
      snprintf(combined, sizeof(combined), "%s: %s",
               radio_app_menu_label(item), value);
      scrolling_text(shown, sizeof(shown), combined, sizeof(combined), 12U,
                     now_ms);
    } else {
      scrolling_text(shown, sizeof(shown), radio_app_menu_label(item), 40U,
                     12U, 0U);
    }
    pcd8544_text(ui->lcd, 9, y, shown, 1U, true);
    if (item == ui->selected) {
      pcd8544_invert_rect(ui->lcd, 0, y, PCD8544_WIDTH, 8);
    }
  }
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

void lcd_ui_handle_input(lcd_ui_t *ui, radio_app_t *app,
                         input_event_t event, uint32_t now_ms) {
  if (ui == NULL || app == NULL) return;
  if (ui->screen == LCD_UI_HOME) {
    if (event.rotation != 0) radio_app_step_frequency(app, event.rotation, now_ms);
    if (event.click) {
      ui->screen = LCD_UI_MENU;
      ui->editing = false;
    }
    if (event.long_press) radio_app_toggle_mute(app, now_ms);
  } else if (ui->editing) {
    if (event.rotation != 0) {
      radio_app_menu_adjust(app, ui->selected, event.rotation, now_ms);
    }
    if (event.click || event.long_press) ui->editing = false;
  } else {
    if (event.rotation != 0) {
      int32_t position = ((int32_t)ui->selected + event.rotation) %
                         RADIO_MENU_COUNT;
      if (position < 0) position += RADIO_MENU_COUNT;
      ui->selected = (radio_menu_item_t)position;
    }
    if (event.click) {
      if (radio_app_menu_is_action(ui->selected)) {
        radio_app_menu_activate(app, ui->selected, now_ms);
      } else {
        ui->editing = true;
      }
    }
    if (event.long_press) ui->screen = LCD_UI_HOME;
  }
  ui->rendered_revision = 0U;
}

void lcd_ui_render(lcd_ui_t *ui, radio_app_t *app, uint32_t now_ms) {
  const radio_settings_t *s;
  if (ui == NULL || app == NULL || ui->lcd == NULL) return;
  if ((uint32_t)(now_ms - ui->last_render_ms) < LCD_RENDER_INTERVAL_MS) return;
  ui->last_render_ms = now_ms;
  s = &app->settings;

  if (ui->configured_contrast != s->lcd_contrast ||
      ui->configured_bias != s->lcd_bias ||
      ui->configured_inverted != s->lcd_inverted ||
      ui->configured_backlight != s->lcd_backlight) {
    pcd8544_configure(ui->lcd, s->lcd_contrast, s->lcd_bias,
                      s->lcd_inverted, s->lcd_backlight);
    ui->configured_contrast = s->lcd_contrast;
    ui->configured_bias = s->lcd_bias;
    ui->configured_inverted = s->lcd_inverted;
    ui->configured_backlight = s->lcd_backlight;
  }

  pcd8544_clear(ui->lcd);
  if (ui->screen == LCD_UI_HOME) render_home(ui, app, now_ms);
  else render_menu(ui, app, now_ms);
  pcd8544_update(ui->lcd);
  ui->rendered_revision = app->revision;
}

#include "radio_app.h"

#include <stdio.h>
#include <string.h>

#define RADIO_POLL_INTERVAL_MS 50U
#define SETTINGS_SAVE_DELAY_MS 3000U
#define RADIO_RETRY_INTERVAL_MS 2000U

static uint8_t clamp_u8(int32_t value, uint8_t maximum) {
  if (value < 0) return 0U;
  if (value > maximum) return maximum;
  return (uint8_t)value;
}

static uint8_t wrap_u8(int32_t value, uint8_t count) {
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return (uint8_t)value;
}

static void changed(radio_app_t *app, uint32_t now_ms, bool tune,
                    bool update_radio) {
  radio_settings_sanitize(&app->settings);
  app->settings_dirty = true;
  app->last_change_ms = now_ms;
  ++app->revision;
  if (!app->radio_available || !update_radio) return;
  if (tune) rda5807_tune(&app->radio, &app->settings);
  else rda5807_apply_settings(&app->radio, &app->settings);
}

void radio_app_init(radio_app_t *app, I2C_HandleTypeDef *i2c,
                    uint32_t now_ms) {
  if (app == NULL) return;
  memset(app, 0, sizeof(*app));
  settings_store_init(&app->store);
  settings_store_load(&app->store, &app->settings);
  radio_settings_sanitize(&app->settings);
  rds_decoder_init(&app->rds);
  app->radio_available =
      rda5807_init(&app->radio, i2c, &app->settings) == RDA5807_OK;
  app->next_radio_retry_ms = now_ms + RADIO_RETRY_INTERVAL_MS;
  app->last_save_ok = true;
  app->revision = 1U;
}

void radio_app_process(radio_app_t *app, uint32_t now_ms) {
  rda5807_operation_t previous_operation;
  if (app == NULL) return;

  if (!app->radio_available) {
    if ((int32_t)(now_ms - app->next_radio_retry_ms) >= 0) {
      app->radio_available = rda5807_init(
          &app->radio, app->radio.i2c, &app->settings) == RDA5807_OK;
      app->next_radio_retry_ms = now_ms + RADIO_RETRY_INTERVAL_MS;
      ++app->revision;
    }
  } else if ((uint32_t)(now_ms - app->last_radio_poll_ms) >=
             RADIO_POLL_INTERVAL_MS) {
    app->last_radio_poll_ms = now_ms;
    previous_operation = app->radio.operation;
    if (rda5807_poll(&app->radio, &app->settings) == RDA5807_OK) {
      if ((previous_operation == RDA5807_OPERATION_SEEKING_UP ||
           previous_operation == RDA5807_OPERATION_SEEKING_DOWN) &&
          app->radio.operation == RDA5807_OPERATION_IDLE &&
          !app->radio.status.seek_failed) {
        app->settings.frequency_khz = radio_frequency_clamp(
            &app->settings, (int32_t)app->radio.status.frequency_khz);
        rds_decoder_reset_station(&app->rds);
        changed(app, now_ms, false, false);
      }
      if (app->settings.rds_enabled && app->radio.status.rds_ready) {
        rds_decoder_process(&app->rds, app->radio.status.rds_blocks,
                            app->radio.status.bler_a,
                            app->radio.status.bler_b);
      }
      ++app->revision;
    } else if (!app->radio.online) {
      app->radio_available = false;
      app->next_radio_retry_ms = now_ms + RADIO_RETRY_INTERVAL_MS;
      ++app->revision;
    }
  }

  if (app->settings_dirty &&
      (uint32_t)(now_ms - app->last_change_ms) >= SETTINGS_SAVE_DELAY_MS) {
    app->last_save_ok = settings_store_save(&app->store, &app->settings);
    app->settings_dirty = !app->last_save_ok;
    if (!app->last_save_ok) app->last_change_ms = now_ms;
    ++app->revision;
  }
}

void radio_app_set_frequency(radio_app_t *app, uint32_t frequency_khz,
                             uint32_t now_ms) {
  uint32_t clamped;
  if (app == NULL) return;
  clamped = radio_frequency_clamp(&app->settings, (int32_t)frequency_khz);
  if (clamped == app->settings.frequency_khz) return;
  app->settings.frequency_khz = clamped;
  rds_decoder_reset_station(&app->rds);
  changed(app, now_ms, true, true);
}

void radio_app_step_frequency(radio_app_t *app, int16_t detents,
                              uint32_t now_ms) {
  if (app == NULL || detents == 0) return;
  radio_app_set_frequency(
      app,
      radio_frequency_step_channels(&app->settings,
                                    app->settings.frequency_khz, detents),
      now_ms);
}

void radio_app_toggle_mute(radio_app_t *app, uint32_t now_ms) {
  if (app == NULL) return;
  app->settings.muted = !app->settings.muted;
  changed(app, now_ms, false, true);
}

void radio_app_seek(radio_app_t *app, bool upwards, uint32_t now_ms) {
  if (app == NULL || !app->radio_available) return;
  rds_decoder_reset_station(&app->rds);
  app->radio.status.seek_failed = false;
  rda5807_seek(&app->radio, &app->settings, upwards);
  app->last_change_ms = now_ms;
  ++app->revision;
}

const char *radio_app_menu_label(radio_menu_item_t item) {
  static const char *const labels[RADIO_MENU_COUNT] = {
      "Czestotliwosc", "Glosnosc", "Wyciszenie", "Tryb audio", "Podbicie basu",
      "Szukaj w gore", "Szukaj w dol", "Pasmo", "Dolne pasmo", "Krok kanalu",
      "Dekoder RDS", "Standard RDS", "Deemfaza", "Koniec szukania",
      "Alg. szukania", "Prog szukania", "Stary prog", "Soft mute",
      "Soft blend", "Prog soft blend", "AFC", "Nowy demodulator",
      "Wejscie LNA", "Prad LNA", "Kontrast LCD", "Negatyw LCD",
      "Podswietlenie", "Ustawienia domyslne"};
  return item < RADIO_MENU_COUNT ? labels[item] : "?";
}

void radio_app_menu_value(const radio_app_t *app, radio_menu_item_t item,
                          char *buffer, size_t buffer_size) {
  static const char *const bands[] = {"87-108 MHz", "76-91 MHz",
                                      "76-108 MHz", "50/65-76 MHz"};
  static const char *const spacings[] = {"100 kHz", "200 kHz", "50 kHz", "25 kHz"};
  static const char *const lna_inputs[] = {"wylaczone", "LNAN", "LNAP", "dual"};
  static const char *const lna_currents[] = {"1.8 mA", "2.1 mA", "2.5 mA", "3.0 mA"};
  const radio_settings_t *s;
  if (app == NULL || buffer == NULL || buffer_size == 0U) return;
  s = &app->settings;
  buffer[0] = '\0';
  switch (item) {
    case RADIO_MENU_FREQUENCY:
      snprintf(buffer, buffer_size, "%lu.%03lu MHz",
               (unsigned long)(s->frequency_khz / 1000U),
               (unsigned long)(s->frequency_khz % 1000U)); break;
    case RADIO_MENU_VOLUME: snprintf(buffer, buffer_size, "%u / 15", s->volume); break;
    case RADIO_MENU_MUTE: snprintf(buffer, buffer_size, "%s", s->muted ? "TAK" : "NIE"); break;
    case RADIO_MENU_AUDIO_MODE: snprintf(buffer, buffer_size, "%s", s->force_mono ? "MONO" : "AUTO STEREO"); break;
    case RADIO_MENU_BASS: snprintf(buffer, buffer_size, "%s", s->bass_boost ? "WL." : "WYL."); break;
    case RADIO_MENU_SEEK_UP:
    case RADIO_MENU_SEEK_DOWN: snprintf(buffer, buffer_size, "nacisnij"); break;
    case RADIO_MENU_BAND: snprintf(buffer, buffer_size, "%s", bands[s->band & 3U]); break;
    case RADIO_MENU_EAST_LIMIT: snprintf(buffer, buffer_size, "%s", s->east_band_starts_at_65mhz ? "65 MHz" : "50 MHz"); break;
    case RADIO_MENU_SPACING: snprintf(buffer, buffer_size, "%s", spacings[s->spacing & 3U]); break;
    case RADIO_MENU_RDS: snprintf(buffer, buffer_size, "%s", s->rds_enabled ? "WL." : "WYL."); break;
    case RADIO_MENU_RBDS: snprintf(buffer, buffer_size, "%s", s->rbds_enabled ? "RBDS" : "RDS"); break;
    case RADIO_MENU_DEEMPHASIS: snprintf(buffer, buffer_size, "%s", s->deemphasis_50us ? "50 us" : "75 us"); break;
    case RADIO_MENU_SEEK_LIMIT: snprintf(buffer, buffer_size, "%s", s->seek_stop_at_band ? "STOP" : "ZAPETL"); break;
    case RADIO_MENU_SEEK_ALGORITHM: snprintf(buffer, buffer_size, "%s", s->seek_mode == RADIO_SEEK_RSSI ? "RSSI" : "SNR"); break;
    case RADIO_MENU_SEEK_THRESHOLD: snprintf(buffer, buffer_size, "%u / 15", s->seek_threshold); break;
    case RADIO_MENU_OLD_SEEK_THRESHOLD: snprintf(buffer, buffer_size, "%u / 63", s->old_seek_threshold); break;
    case RADIO_MENU_SOFTMUTE: snprintf(buffer, buffer_size, "%s", s->softmute_enabled ? "WL." : "WYL."); break;
    case RADIO_MENU_SOFTBLEND: snprintf(buffer, buffer_size, "%s", s->softblend_enabled ? "WL." : "WYL."); break;
    case RADIO_MENU_SOFTBLEND_THRESHOLD: snprintf(buffer, buffer_size, "%u (x2 dB)", s->softblend_threshold); break;
    case RADIO_MENU_AFC: snprintf(buffer, buffer_size, "%s", s->afc_enabled ? "WL." : "WYL."); break;
    case RADIO_MENU_NEW_METHOD: snprintf(buffer, buffer_size, "%s", s->new_method_enabled ? "WL." : "WYL."); break;
    case RADIO_MENU_LNA_INPUT: snprintf(buffer, buffer_size, "%s", lna_inputs[s->lna_port & 3U]); break;
    case RADIO_MENU_LNA_CURRENT: snprintf(buffer, buffer_size, "%s", lna_currents[s->lna_current & 3U]); break;
    case RADIO_MENU_LCD_CONTRAST: snprintf(buffer, buffer_size, "%u / 127", s->lcd_contrast); break;
    case RADIO_MENU_LCD_INVERT: snprintf(buffer, buffer_size, "%s", s->lcd_inverted ? "TAK" : "NIE"); break;
    case RADIO_MENU_LCD_BACKLIGHT: snprintf(buffer, buffer_size, "%s", s->lcd_backlight ? "WL." : "WYL."); break;
    case RADIO_MENU_DEFAULTS: snprintf(buffer, buffer_size, "nacisnij"); break;
    default: break;
  }
}

bool radio_app_menu_is_action(radio_menu_item_t item) {
  return item == RADIO_MENU_SEEK_UP || item == RADIO_MENU_SEEK_DOWN ||
         item == RADIO_MENU_DEFAULTS;
}

void radio_app_menu_adjust(radio_app_t *app, radio_menu_item_t item,
                           int16_t delta, uint32_t now_ms) {
  radio_settings_t *s;
  bool tune = false;
  bool update_radio = true;
  if (app == NULL || delta == 0 || radio_app_menu_is_action(item)) return;
  s = &app->settings;
  switch (item) {
    case RADIO_MENU_FREQUENCY:
      radio_app_step_frequency(app, delta, now_ms); return;
    case RADIO_MENU_VOLUME: s->volume = clamp_u8((int32_t)s->volume + delta, 15U); break;
    case RADIO_MENU_MUTE: s->muted = !s->muted; break;
    case RADIO_MENU_AUDIO_MODE: s->force_mono = !s->force_mono; break;
    case RADIO_MENU_BASS: s->bass_boost = !s->bass_boost; break;
    case RADIO_MENU_BAND: s->band = wrap_u8((int32_t)s->band + delta, 4U); tune = true; break;
    case RADIO_MENU_EAST_LIMIT: s->east_band_starts_at_65mhz = !s->east_band_starts_at_65mhz; tune = true; break;
    case RADIO_MENU_SPACING: s->spacing = wrap_u8((int32_t)s->spacing + delta, 4U); tune = true; break;
    case RADIO_MENU_RDS: s->rds_enabled = !s->rds_enabled; break;
    case RADIO_MENU_RBDS: s->rbds_enabled = !s->rbds_enabled; break;
    case RADIO_MENU_DEEMPHASIS: s->deemphasis_50us = !s->deemphasis_50us; break;
    case RADIO_MENU_SEEK_LIMIT: s->seek_stop_at_band = !s->seek_stop_at_band; break;
    case RADIO_MENU_SEEK_ALGORITHM: s->seek_mode = s->seek_mode == RADIO_SEEK_RSSI ? RADIO_SEEK_SNR : RADIO_SEEK_RSSI; break;
    case RADIO_MENU_SEEK_THRESHOLD: s->seek_threshold = clamp_u8((int32_t)s->seek_threshold + delta, 15U); break;
    case RADIO_MENU_OLD_SEEK_THRESHOLD: s->old_seek_threshold = clamp_u8((int32_t)s->old_seek_threshold + delta, 63U); break;
    case RADIO_MENU_SOFTMUTE: s->softmute_enabled = !s->softmute_enabled; break;
    case RADIO_MENU_SOFTBLEND: s->softblend_enabled = !s->softblend_enabled; break;
    case RADIO_MENU_SOFTBLEND_THRESHOLD: s->softblend_threshold = clamp_u8((int32_t)s->softblend_threshold + delta, 31U); break;
    case RADIO_MENU_AFC: s->afc_enabled = !s->afc_enabled; break;
    case RADIO_MENU_NEW_METHOD: s->new_method_enabled = !s->new_method_enabled; break;
    case RADIO_MENU_LNA_INPUT: s->lna_port = wrap_u8((int32_t)s->lna_port + delta, 4U); break;
    case RADIO_MENU_LNA_CURRENT: s->lna_current = wrap_u8((int32_t)s->lna_current + delta, 4U); break;
    case RADIO_MENU_LCD_CONTRAST: s->lcd_contrast = clamp_u8((int32_t)s->lcd_contrast + delta, 127U); update_radio = false; break;
    case RADIO_MENU_LCD_INVERT: s->lcd_inverted = !s->lcd_inverted; update_radio = false; break;
    case RADIO_MENU_LCD_BACKLIGHT: s->lcd_backlight = !s->lcd_backlight; update_radio = false; break;
    default: return;
  }
  if (tune) rds_decoder_reset_station(&app->rds);
  changed(app, now_ms, tune, update_radio);
}

void radio_app_menu_activate(radio_app_t *app, radio_menu_item_t item,
                             uint32_t now_ms) {
  if (app == NULL) return;
  if (item == RADIO_MENU_SEEK_UP || item == RADIO_MENU_SEEK_DOWN) {
    radio_app_seek(app, item == RADIO_MENU_SEEK_UP, now_ms);
  } else if (item == RADIO_MENU_DEFAULTS) {
    radio_settings_defaults(&app->settings);
    rds_decoder_reset_station(&app->rds);
    changed(app, now_ms, true, true);
  }
}

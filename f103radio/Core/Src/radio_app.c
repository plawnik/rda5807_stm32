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

static void copy_station_name(char destination[RADIO_STATION_NAME_LENGTH + 1U],
                              const char *source) {
  uint8_t index = 0U;
  memset(destination, 0, RADIO_STATION_NAME_LENGTH + 1U);
  if (source == NULL) return;
  while (index < RADIO_STATION_NAME_LENGTH && source[index] != '\0') {
    const uint8_t value = (uint8_t)source[index];
    destination[index] =
        value >= 0x20U && value <= 0x7EU ? (char)value : ' ';
    ++index;
  }
}

static void reset_rssi_average(radio_app_t *app, uint32_t now_ms) {
  app->rssi_sum = 0U;
  app->rssi_samples = 0U;
  app->rssi_window_started_ms = now_ms;
}

static void update_rssi_average(radio_app_t *app, uint32_t now_ms) {
  app->rssi_sum += app->radio.status.rssi;
  if (app->rssi_samples < UINT16_MAX) ++app->rssi_samples;
  if (!app->averaged_rssi_valid) {
    app->averaged_rssi = app->radio.status.rssi;
    app->averaged_rssi_valid = true;
  }
  if ((uint32_t)(now_ms - app->rssi_window_started_ms) >=
          app->settings.rssi_average_ms &&
      app->rssi_samples != 0U) {
    app->averaged_rssi = (uint8_t)(
        (app->rssi_sum + app->rssi_samples / 2U) / app->rssi_samples);
    reset_rssi_average(app, now_ms);
  }
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
  reset_rssi_average(app, now_ms);
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
      update_rssi_average(app, now_ms);
      if ((previous_operation == RDA5807_OPERATION_SEEKING_UP ||
           previous_operation == RDA5807_OPERATION_SEEKING_DOWN) &&
          app->radio.operation == RDA5807_OPERATION_IDLE &&
          !app->radio.status.seek_failed) {
        radio_settings_plan_frequency(
            &app->settings, (int32_t)app->radio.status.frequency_khz,
            radio_spacing_khz(&app->settings));
        app->active_station_valid = false;
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
  if (app == NULL) return;
  radio_app_set_frequency_auto(
      app, frequency_khz > (uint32_t)INT32_MAX ? INT32_MAX
                                               : (int32_t)frequency_khz,
      radio_spacing_khz(&app->settings), now_ms);
}

void radio_app_set_frequency_auto(radio_app_t *app, int32_t frequency_khz,
                                  uint16_t requested_spacing_khz,
                                  uint32_t now_ms) {
  uint32_t previous_frequency;
  uint8_t previous_band;
  uint8_t previous_spacing;
  bool previous_east_limit;
  bool previous_automatic;
  if (app == NULL) return;
  previous_frequency = app->settings.frequency_khz;
  previous_band = app->settings.band;
  previous_spacing = app->settings.spacing;
  previous_east_limit = app->settings.east_band_starts_at_65mhz;
  previous_automatic = app->settings.automatic_tuning;
  radio_settings_plan_frequency(&app->settings, frequency_khz,
                                requested_spacing_khz);
  if (app->settings.frequency_khz == previous_frequency &&
      app->settings.band == previous_band &&
      app->settings.spacing == previous_spacing &&
      app->settings.east_band_starts_at_65mhz == previous_east_limit &&
      app->settings.automatic_tuning == previous_automatic) {
    return;
  }
  app->active_station_valid = false;
  rds_decoder_reset_station(&app->rds);
  changed(app, now_ms, true, true);
}

void radio_app_set_extended_tuning(radio_app_t *app, bool enabled,
                                   uint32_t now_ms) {
  uint32_t previous_frequency;
  uint8_t previous_band;
  uint8_t previous_spacing;
  if (app == NULL || app->settings.extended_tuning == enabled) return;
  previous_frequency = app->settings.frequency_khz;
  previous_band = app->settings.band;
  previous_spacing = app->settings.spacing;
  app->settings.extended_tuning = enabled;
  radio_settings_plan_frequency(&app->settings,
                                (int32_t)app->settings.frequency_khz,
                                radio_spacing_khz(&app->settings));
  if (app->settings.frequency_khz != previous_frequency ||
      app->settings.band != previous_band ||
      app->settings.spacing != previous_spacing) {
    rds_decoder_reset_station(&app->rds);
    changed(app, now_ms, true, true);
  } else {
    changed(app, now_ms, false, false);
  }
}

void radio_app_step_frequency(radio_app_t *app, int16_t detents,
                              uint32_t now_ms) {
  int64_t target;
  uint16_t spacing;
  if (app == NULL || detents == 0) return;
  spacing = radio_spacing_khz(&app->settings);
  target = (int64_t)app->settings.frequency_khz +
           (int64_t)detents * spacing;
  if (target > INT32_MAX) target = INT32_MAX;
  if (target < INT32_MIN) target = INT32_MIN;
  radio_app_set_frequency_auto(app, (int32_t)target, spacing, now_ms);
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

uint8_t radio_app_display_rssi(const radio_app_t *app) {
  if (app == NULL) return 0U;
  return app->averaged_rssi_valid ? app->averaged_rssi
                                  : app->radio.status.rssi;
}

bool radio_app_station_tune(radio_app_t *app, uint8_t index,
                            uint32_t now_ms) {
  if (app == NULL || index >= app->settings.station_count) return false;
  radio_app_set_frequency_auto(
      app, (int32_t)app->settings.stations[index].frequency_khz, 50U, now_ms);
  app->active_station_index = index;
  app->active_station_valid = true;
  return true;
}

void radio_app_control_left_right(radio_app_t *app, uint8_t action,
                                  int16_t direction, uint32_t now_ms) {
  int32_t delta;
  uint8_t index;
  uint16_t steps;
  if (app == NULL || direction == 0) return;
  switch ((radio_control_action_t)action) {
    case RADIO_CONTROL_TUNE_50_KHZ:
    case RADIO_CONTROL_TUNE_100_KHZ:
      delta = (int32_t)direction *
              (action == RADIO_CONTROL_TUNE_50_KHZ ? 50 : 100);
      radio_app_set_frequency_auto(
          app, (int32_t)app->settings.frequency_khz + delta,
          action == RADIO_CONTROL_TUNE_50_KHZ ? 50U : 100U, now_ms);
      break;
    case RADIO_CONTROL_SEEK:
      radio_app_seek(app, direction > 0, now_ms);
      break;
    case RADIO_CONTROL_STATIONS:
      if (app->settings.station_count == 0U) return;
      if (app->active_station_valid &&
          app->active_station_index < app->settings.station_count) {
        index = app->active_station_index;
      } else {
        bool found = false;
        index = 0U;
        for (uint8_t station = 0U;
             station < app->settings.station_count; ++station) {
          if (app->settings.stations[station].frequency_khz ==
              app->settings.frequency_khz) {
            index = station;
            found = true;
            break;
          }
        }
        if (!found) {
          index = direction > 0 ? (uint8_t)(app->settings.station_count - 1U)
                                : 0U;
        }
      }
      steps = (uint16_t)(direction > 0 ? direction : -direction);
      while (steps-- != 0U) {
        if (direction > 0) {
          index = (uint8_t)((index + 1U) % app->settings.station_count);
        } else {
          index = index == 0U ? (uint8_t)(app->settings.station_count - 1U)
                              : (uint8_t)(index - 1U);
        }
      }
      radio_app_station_tune(app, index, now_ms);
      break;
    case RADIO_CONTROL_ACTION_COUNT:
    default:
      break;
  }
}

bool radio_app_station_add(radio_app_t *app, uint32_t frequency_khz,
                           const char *name, uint32_t now_ms) {
  radio_station_t *station;
  if (app == NULL || app->settings.station_count >= RADIO_MAX_STATIONS) {
    return false;
  }
  station = &app->settings.stations[app->settings.station_count];
  memset(station, 0, sizeof(*station));
  station->frequency_khz = frequency_khz;
  copy_station_name(station->name, name);
  ++app->settings.station_count;
  changed(app, now_ms, false, false);
  return true;
}

bool radio_app_station_update(radio_app_t *app, uint8_t index,
                              uint32_t frequency_khz, const char *name,
                              uint32_t now_ms) {
  radio_station_t *station;
  if (app == NULL || index >= app->settings.station_count) return false;
  station = &app->settings.stations[index];
  station->frequency_khz = frequency_khz;
  if (name != NULL) copy_station_name(station->name, name);
  changed(app, now_ms, false, false);
  app->active_station_valid = false;
  return true;
}

bool radio_app_station_delete(radio_app_t *app, uint8_t index,
                              uint32_t now_ms) {
  if (app == NULL || index >= app->settings.station_count) return false;
  if (index + 1U < app->settings.station_count) {
    memmove(&app->settings.stations[index],
            &app->settings.stations[index + 1U],
            (size_t)(app->settings.station_count - index - 1U) *
                sizeof(app->settings.stations[0]));
  }
  --app->settings.station_count;
  memset(&app->settings.stations[app->settings.station_count], 0,
         sizeof(app->settings.stations[0]));
  app->active_station_valid = false;
  changed(app, now_ms, false, false);
  return true;
}

bool radio_app_flush_settings(radio_app_t *app) {
  if (app == NULL) return false;
  if (!app->settings_dirty) return true;
  app->last_save_ok = settings_store_save(&app->store, &app->settings);
  app->settings_dirty = !app->last_save_ok;
  ++app->revision;
  return app->last_save_ok;
}

void radio_app_power_down(radio_app_t *app) {
  if (app == NULL) return;
  radio_app_flush_settings(app);
  if (app->radio_available) rda5807_power_down(&app->radio);
  app->radio_available = false;
  ++app->revision;
}

void radio_app_wake(radio_app_t *app, uint32_t now_ms) {
  I2C_HandleTypeDef *i2c;
  if (app == NULL) return;
  i2c = app->radio.i2c;
  rds_decoder_reset_station(&app->rds);
  app->radio_available =
      rda5807_init(&app->radio, i2c, &app->settings) == RDA5807_OK;
  app->next_radio_retry_ms = now_ms + RADIO_RETRY_INTERVAL_MS;
  app->last_radio_poll_ms = now_ms;
  app->averaged_rssi_valid = false;
  reset_rssi_average(app, now_ms);
  ++app->revision;
}

const char *radio_app_menu_label(radio_menu_item_t item) {
  static const char *const labels[RADIO_MENU_COUNT] = {
      "Czestotliwosc", "Zakres rozszerzony", "Glosnosc", "Wyciszenie",
      "Tryb audio", "Podbicie basu", "Szukaj w gore", "Szukaj w dol",
      "Pasmo", "Dolne pasmo", "Krok kanalu", "Dekoder RDS",
      "Standard RDS", "Deemfaza", "Koniec szukania", "Alg. szukania",
      "Prog szukania", "Usrednianie RSSI", "Stary prog", "Soft mute",
      "Soft blend", "Prog soft blend", "AFC", "Nowy demodulator",
      "Wejscie LNA", "Prad LNA", "Kontrast LCD", "Negatyw LCD",
      "Podswietlenie", "Ruch enkodera", "Przyciski L/P",
      "Sterowanie", "Ustawienia domyslne"};
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
    case RADIO_MENU_EXTENDED_RANGE:
      snprintf(buffer, buffer_size, "%s", s->extended_tuning
                   ? "WL. (291.6 MHz)" : "WYL. (115 MHz)"); break;
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
    case RADIO_MENU_RSSI_AVERAGE:
      snprintf(buffer, buffer_size, "%u.%u s",
               s->rssi_average_ms / 1000U,
               (s->rssi_average_ms % 1000U) / 100U); break;
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
    case RADIO_MENU_ENCODER_ACTION:
      snprintf(buffer, buffer_size, "%s",
               radio_control_action_name(s->encoder_action)); break;
    case RADIO_MENU_BUTTON_ACTION:
      snprintf(buffer, buffer_size, "%s",
               radio_control_action_name(s->buttons_action)); break;
    case RADIO_MENU_INPUT_MODE:
      snprintf(buffer, buffer_size, "%s",
               radio_input_mode_name(s->input_mode)); break;
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
    case RADIO_MENU_EXTENDED_RANGE:
      radio_app_set_extended_tuning(app, !s->extended_tuning, now_ms); return;
    case RADIO_MENU_VOLUME: s->volume = clamp_u8((int32_t)s->volume + delta, 15U); break;
    case RADIO_MENU_MUTE: s->muted = !s->muted; break;
    case RADIO_MENU_AUDIO_MODE: s->force_mono = !s->force_mono; break;
    case RADIO_MENU_BASS: s->bass_boost = !s->bass_boost; break;
    case RADIO_MENU_BAND:
      s->automatic_tuning = false;
      s->band = wrap_u8((int32_t)s->band + delta, 4U);
      tune = true;
      break;
    case RADIO_MENU_EAST_LIMIT:
      s->automatic_tuning = false;
      s->east_band_starts_at_65mhz = !s->east_band_starts_at_65mhz;
      tune = true;
      break;
    case RADIO_MENU_SPACING:
      s->automatic_tuning = false;
      s->spacing = wrap_u8((int32_t)s->spacing + delta, 4U);
      tune = true;
      break;
    case RADIO_MENU_RDS: s->rds_enabled = !s->rds_enabled; break;
    case RADIO_MENU_RBDS: s->rbds_enabled = !s->rbds_enabled; break;
    case RADIO_MENU_DEEMPHASIS: s->deemphasis_50us = !s->deemphasis_50us; break;
    case RADIO_MENU_SEEK_LIMIT: s->seek_stop_at_band = !s->seek_stop_at_band; break;
    case RADIO_MENU_SEEK_ALGORITHM: s->seek_mode = s->seek_mode == RADIO_SEEK_RSSI ? RADIO_SEEK_SNR : RADIO_SEEK_RSSI; break;
    case RADIO_MENU_SEEK_THRESHOLD: s->seek_threshold = clamp_u8((int32_t)s->seek_threshold + delta, 15U); break;
    case RADIO_MENU_RSSI_AVERAGE: {
      int32_t value = (int32_t)s->rssi_average_ms +
                      (int32_t)delta * RADIO_RSSI_AVERAGE_STEP_MS;
      if (value < (int32_t)RADIO_RSSI_AVERAGE_MIN_MS) {
        value = RADIO_RSSI_AVERAGE_MIN_MS;
      }
      if (value > (int32_t)RADIO_RSSI_AVERAGE_MAX_MS) {
        value = RADIO_RSSI_AVERAGE_MAX_MS;
      }
      s->rssi_average_ms = (uint16_t)value;
      reset_rssi_average(app, now_ms);
      break;
    }
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
    case RADIO_MENU_ENCODER_ACTION:
      s->encoder_action = wrap_u8((int32_t)s->encoder_action + delta,
                                  RADIO_CONTROL_ACTION_COUNT);
      update_radio = false;
      break;
    case RADIO_MENU_BUTTON_ACTION:
      s->buttons_action = wrap_u8((int32_t)s->buttons_action + delta,
                                  RADIO_CONTROL_ACTION_COUNT);
      update_radio = false;
      break;
    case RADIO_MENU_INPUT_MODE:
      s->input_mode = wrap_u8((int32_t)s->input_mode + delta,
                              RADIO_INPUT_MODE_COUNT);
      update_radio = false;
      break;
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

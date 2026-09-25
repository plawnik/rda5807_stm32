#include "app_config.h"

#include <string.h>

void radio_settings_defaults(radio_settings_t *settings) {
  if (settings == NULL) {
    return;
  }

  memset(settings, 0, sizeof(*settings));
  settings->frequency_khz = 106100U;
  settings->volume = 8U;
  settings->band = RADIO_BAND_EU_US;
  settings->spacing = RADIO_SPACING_100_KHZ;
  settings->seek_threshold = 8U;
  settings->old_seek_threshold = 16U;
  settings->softblend_threshold = 16U;
  settings->seek_mode = RADIO_SEEK_SNR;
  settings->lna_port = 2U; /* LNAP, recommended for a single-ended input. */
  settings->lna_current = 0U;
  settings->lcd_contrast = 56U;
  settings->lcd_bias = 4U;
  settings->rds_enabled = true;
  settings->deemphasis_50us = true;
  settings->softmute_enabled = true;
  settings->softblend_enabled = true;
  settings->afc_enabled = true;
  settings->new_method_enabled = true;
  settings->east_band_starts_at_65mhz = true;
  settings->automatic_tuning = true;
  settings->lcd_backlight = true;
}

static uint8_t spacing_register_value(uint16_t requested_spacing_khz) {
  if (requested_spacing_khz <= 25U) return RADIO_SPACING_25_KHZ;
  if (requested_spacing_khz <= 50U) return RADIO_SPACING_50_KHZ;
  if (requested_spacing_khz <= 100U) return RADIO_SPACING_100_KHZ;
  return RADIO_SPACING_200_KHZ;
}

static uint8_t next_wider_spacing(uint8_t spacing) {
  switch ((radio_spacing_t)spacing) {
    case RADIO_SPACING_25_KHZ: return RADIO_SPACING_50_KHZ;
    case RADIO_SPACING_50_KHZ: return RADIO_SPACING_100_KHZ;
    case RADIO_SPACING_100_KHZ:
    case RADIO_SPACING_200_KHZ:
    default: return RADIO_SPACING_200_KHZ;
  }
}

uint32_t radio_band_min_khz(const radio_settings_t *settings) {
  switch ((radio_band_t)settings->band) {
    case RADIO_BAND_JAPAN:
    case RADIO_BAND_WORLD:
      return 76000U;
    case RADIO_BAND_EAST:
      return settings->east_band_starts_at_65mhz ? 65000U : 50000U;
    case RADIO_BAND_EU_US:
    default:
      return 87000U;
  }
}

uint32_t radio_band_max_khz(const radio_settings_t *settings) {
  switch ((radio_band_t)settings->band) {
    case RADIO_BAND_JAPAN:
      return 91000U;
    case RADIO_BAND_EAST:
      return 76000U;
    case RADIO_BAND_WORLD:
    case RADIO_BAND_EU_US:
    default:
      return 108000U;
  }
}

uint16_t radio_spacing_khz(const radio_settings_t *settings) {
  static const uint16_t spacing[] = {100U, 200U, 50U, 25U};
  return spacing[settings->spacing & 0x03U];
}

uint32_t radio_frequency_clamp(const radio_settings_t *settings,
                               int32_t frequency_khz) {
  const uint32_t low = radio_band_min_khz(settings);
  const uint32_t high = radio_band_max_khz(settings);
  const uint32_t step = radio_spacing_khz(settings);
  uint32_t result;

  if (frequency_khz <= (int32_t)low) {
    return low;
  }
  if (frequency_khz >= (int32_t)high) {
    return high;
  }

  result = (uint32_t)frequency_khz;
  result = low + (((result - low) + (step / 2U)) / step) * step;
  return result > high ? high : result;
}

uint32_t radio_frequency_step(const radio_settings_t *settings,
                              uint32_t frequency_khz, int32_t delta_khz) {
  int64_t next = (int64_t)frequency_khz + delta_khz;
  const int64_t low = radio_band_min_khz(settings);
  const int64_t high = radio_band_max_khz(settings);

  if (next > high) {
    next = settings->seek_stop_at_band ? high : low;
  } else if (next < low) {
    next = settings->seek_stop_at_band ? low : high;
  }
  return radio_frequency_clamp(settings, (int32_t)next);
}

uint32_t radio_frequency_step_channels(const radio_settings_t *settings,
                                       uint32_t frequency_khz,
                                       int32_t channel_delta) {
  const int64_t delta_khz =
      (int64_t)channel_delta * radio_spacing_khz(settings);
  if (delta_khz > INT32_MAX) {
    return radio_frequency_step(settings, frequency_khz, INT32_MAX);
  }
  if (delta_khz < INT32_MIN) {
    return radio_frequency_step(settings, frequency_khz, INT32_MIN);
  }
  return radio_frequency_step(settings, frequency_khz, (int32_t)delta_khz);
}

uint32_t radio_settings_plan_frequency(radio_settings_t *settings,
                                       int32_t frequency_khz,
                                       uint16_t requested_spacing_khz) {
  uint32_t requested;
  uint32_t base;
  uint32_t step;
  uint32_t channel;
  const uint32_t maximum = settings != NULL && settings->extended_tuning
                               ? RADIO_REGISTER_MAX_KHZ
                               : RADIO_SYNTH_MAX_KHZ;

  if (settings == NULL) return RADIO_SYNTH_MIN_KHZ;
  if (frequency_khz <= (int32_t)RADIO_SYNTH_MIN_KHZ) {
    requested = RADIO_SYNTH_MIN_KHZ;
  } else if (frequency_khz >= (int32_t)maximum) {
    requested = maximum;
  } else {
    requested = (uint32_t)frequency_khz;
  }

  if (requested < 76000U) {
    settings->band = RADIO_BAND_EAST;
    settings->east_band_starts_at_65mhz = false;
    base = 50000U;
  } else if (requested < 87000U) {
    settings->band = RADIO_BAND_WORLD;
    base = 76000U;
  } else {
    settings->band = RADIO_BAND_EU_US;
    base = 87000U;
  }

  settings->spacing = spacing_register_value(requested_spacing_khz);
  step = radio_spacing_khz(settings);
  channel = ((requested - base) + (step / 2U)) / step;
  while (channel > 0x03FFU && step < 200U) {
    settings->spacing = next_wider_spacing(settings->spacing);
    step = radio_spacing_khz(settings);
    channel = ((requested - base) + (step / 2U)) / step;
  }
  if (channel > 0x03FFU) channel = 0x03FFU;

  settings->frequency_khz = base + channel * step;
  if (settings->frequency_khz > maximum) {
    channel = (maximum - base) / step;
    settings->frequency_khz = base + channel * step;
  }
  settings->automatic_tuning = true;
  return settings->frequency_khz;
}

void radio_settings_sanitize(radio_settings_t *settings) {
  if (settings == NULL) {
    return;
  }

  if (settings->band > RADIO_BAND_EAST) {
    settings->band = RADIO_BAND_EU_US;
  }
  settings->spacing &= 0x03U;
  if (settings->seek_mode != RADIO_SEEK_RSSI) {
    settings->seek_mode = RADIO_SEEK_SNR;
  }
  if (settings->volume > 15U) settings->volume = 15U;
  if (settings->seek_threshold > 15U) settings->seek_threshold = 15U;
  if (settings->old_seek_threshold > 63U) settings->old_seek_threshold = 63U;
  if (settings->softblend_threshold > 31U) settings->softblend_threshold = 31U;
  if (settings->lna_port > 3U) settings->lna_port = 2U;
  if (settings->lna_current > 3U) settings->lna_current = 0U;
  if (settings->lcd_contrast > 127U) settings->lcd_contrast = 127U;
  if (settings->lcd_contrast < 20U) settings->lcd_contrast = 20U;
  if (settings->lcd_bias > 7U) settings->lcd_bias = 4U;
  if (settings->automatic_tuning) {
    radio_settings_plan_frequency(settings, (int32_t)settings->frequency_khz,
                                  radio_spacing_khz(settings));
  } else {
    settings->frequency_khz = radio_frequency_clamp(
        settings, (int32_t)settings->frequency_khz);
  }
}

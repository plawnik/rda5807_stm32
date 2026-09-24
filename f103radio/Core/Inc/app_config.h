#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RADIO_BAND_EU_US = 0,
  RADIO_BAND_JAPAN = 1,
  RADIO_BAND_WORLD = 2,
  RADIO_BAND_EAST = 3
} radio_band_t;

/* Values intentionally match the SPACE bits in register 0x03. */
typedef enum {
  RADIO_SPACING_100_KHZ = 0,
  RADIO_SPACING_200_KHZ = 1,
  RADIO_SPACING_50_KHZ = 2,
  RADIO_SPACING_25_KHZ = 3
} radio_spacing_t;

typedef enum {
  RADIO_SEEK_SNR = 0,
  RADIO_SEEK_RSSI = 2
} radio_seek_mode_t;

typedef struct {
  uint32_t frequency_khz;
  uint8_t volume;
  uint8_t band;
  uint8_t spacing;
  uint8_t seek_threshold;
  uint8_t old_seek_threshold;
  uint8_t softblend_threshold;
  uint8_t seek_mode;
  uint8_t lna_port;
  uint8_t lna_current;
  uint8_t lcd_contrast;
  uint8_t lcd_bias;
  bool muted;
  bool force_mono;
  bool bass_boost;
  bool rds_enabled;
  bool rbds_enabled;
  bool deemphasis_50us;
  bool softmute_enabled;
  bool softblend_enabled;
  bool afc_enabled;
  bool new_method_enabled;
  bool seek_stop_at_band;
  bool east_band_starts_at_65mhz;
  bool lcd_inverted;
  bool lcd_backlight;
} radio_settings_t;

void radio_settings_defaults(radio_settings_t *settings);
void radio_settings_sanitize(radio_settings_t *settings);
uint32_t radio_band_min_khz(const radio_settings_t *settings);
uint32_t radio_band_max_khz(const radio_settings_t *settings);
uint16_t radio_spacing_khz(const radio_settings_t *settings);
uint32_t radio_frequency_clamp(const radio_settings_t *settings,
                               int32_t frequency_khz);
uint32_t radio_frequency_step(const radio_settings_t *settings,
                              uint32_t frequency_khz, int32_t delta_khz);

#endif /* APP_CONFIG_H */

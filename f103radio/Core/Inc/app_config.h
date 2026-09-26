#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define RADIO_SYNTH_MIN_KHZ 50000U
#define RADIO_SYNTH_MAX_KHZ 115000U
#define RADIO_REGISTER_MAX_KHZ (87000U + (0x03FFU * 200U))
#define RADIO_MAX_STATIONS 12U
#define RADIO_STATION_NAME_LENGTH 8U
#define RADIO_RSSI_AVERAGE_MIN_MS 200U
#define RADIO_RSSI_AVERAGE_MAX_MS 5000U
#define RADIO_RSSI_AVERAGE_STEP_MS 100U

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

typedef enum {
  RADIO_CONTROL_TUNE_50_KHZ = 0,
  RADIO_CONTROL_TUNE_100_KHZ,
  RADIO_CONTROL_SEEK,
  RADIO_CONTROL_STATIONS,
  RADIO_CONTROL_ACTION_COUNT
} radio_control_action_t;

typedef enum {
  RADIO_INPUT_ENCODER = 0,
  RADIO_INPUT_BUTTONS,
  RADIO_INPUT_MODE_COUNT
} radio_input_mode_t;

typedef struct {
  uint32_t frequency_khz;
  char name[RADIO_STATION_NAME_LENGTH + 1U];
  uint8_t reserved[3];
} radio_station_t;

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
  bool automatic_tuning;
  bool extended_tuning;
  bool lcd_inverted;
  bool lcd_backlight;
  uint16_t rssi_average_ms;
  uint8_t encoder_action;
  uint8_t buttons_action;
  uint8_t station_count;
  uint8_t reserved[3];
  radio_station_t stations[RADIO_MAX_STATIONS];
  /* Appended in settings format 4 so format 3 records remain migratable. */
  uint8_t input_mode;
  uint8_t reserved_v4[3];
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
uint32_t radio_frequency_step_channels(const radio_settings_t *settings,
                                       uint32_t frequency_khz,
                                       int32_t channel_delta);
uint32_t radio_settings_plan_frequency(radio_settings_t *settings,
                                       int32_t frequency_khz,
                                       uint16_t requested_spacing_khz);
const char *radio_control_action_name(uint8_t action);
const char *radio_input_mode_name(uint8_t mode);

#endif /* APP_CONFIG_H */

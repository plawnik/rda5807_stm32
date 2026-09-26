#ifndef RADIO_APP_H
#define RADIO_APP_H

#include "app_config.h"
#include "rda5807.h"
#include "rds_decoder.h"
#include "settings_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  RADIO_MENU_FREQUENCY = 0,
  RADIO_MENU_EXTENDED_RANGE,
  RADIO_MENU_VOLUME,
  RADIO_MENU_MUTE,
  RADIO_MENU_AUDIO_MODE,
  RADIO_MENU_BASS,
  RADIO_MENU_SEEK_UP,
  RADIO_MENU_SEEK_DOWN,
  RADIO_MENU_BAND,
  RADIO_MENU_EAST_LIMIT,
  RADIO_MENU_SPACING,
  RADIO_MENU_RDS,
  RADIO_MENU_RBDS,
  RADIO_MENU_DEEMPHASIS,
  RADIO_MENU_SEEK_LIMIT,
  RADIO_MENU_SEEK_ALGORITHM,
  RADIO_MENU_SEEK_THRESHOLD,
  RADIO_MENU_RSSI_AVERAGE,
  RADIO_MENU_OLD_SEEK_THRESHOLD,
  RADIO_MENU_SOFTMUTE,
  RADIO_MENU_SOFTBLEND,
  RADIO_MENU_SOFTBLEND_THRESHOLD,
  RADIO_MENU_AFC,
  RADIO_MENU_NEW_METHOD,
  RADIO_MENU_LNA_INPUT,
  RADIO_MENU_LNA_CURRENT,
  RADIO_MENU_LCD_CONTRAST,
  RADIO_MENU_LCD_INVERT,
  RADIO_MENU_LCD_BACKLIGHT,
  RADIO_MENU_ENCODER_ACTION,
  RADIO_MENU_BUTTON_ACTION,
  RADIO_MENU_INPUT_MODE,
  RADIO_MENU_DEFAULTS,
  RADIO_MENU_COUNT
} radio_menu_item_t;

typedef struct {
  radio_settings_t settings;
  settings_store_t store;
  rda5807_t radio;
  rds_decoder_t rds;
  uint32_t last_radio_poll_ms;
  uint32_t last_change_ms;
  uint32_t next_radio_retry_ms;
  uint32_t rssi_sum;
  uint32_t rssi_window_started_ms;
  uint32_t revision;
  uint16_t rssi_samples;
  uint8_t averaged_rssi;
  uint8_t active_station_index;
  bool settings_dirty;
  bool radio_available;
  bool last_save_ok;
  bool averaged_rssi_valid;
  bool active_station_valid;
} radio_app_t;

void radio_app_init(radio_app_t *app, I2C_HandleTypeDef *i2c,
                    uint32_t now_ms);
void radio_app_process(radio_app_t *app, uint32_t now_ms);
void radio_app_step_frequency(radio_app_t *app, int16_t detents,
                              uint32_t now_ms);
void radio_app_set_frequency(radio_app_t *app, uint32_t frequency_khz,
                             uint32_t now_ms);
void radio_app_set_frequency_auto(radio_app_t *app, int32_t frequency_khz,
                                  uint16_t requested_spacing_khz,
                                  uint32_t now_ms);
void radio_app_set_extended_tuning(radio_app_t *app, bool enabled,
                                   uint32_t now_ms);
void radio_app_toggle_mute(radio_app_t *app, uint32_t now_ms);
void radio_app_seek(radio_app_t *app, bool upwards, uint32_t now_ms);
void radio_app_control_left_right(radio_app_t *app, uint8_t action,
                                  int16_t direction, uint32_t now_ms);
uint8_t radio_app_display_rssi(const radio_app_t *app);
bool radio_app_station_add(radio_app_t *app, uint32_t frequency_khz,
                           const char *name, uint32_t now_ms);
bool radio_app_station_update(radio_app_t *app, uint8_t index,
                              uint32_t frequency_khz, const char *name,
                              uint32_t now_ms);
bool radio_app_station_delete(radio_app_t *app, uint8_t index,
                              uint32_t now_ms);
bool radio_app_station_tune(radio_app_t *app, uint8_t index,
                            uint32_t now_ms);
bool radio_app_flush_settings(radio_app_t *app);
void radio_app_power_down(radio_app_t *app);
void radio_app_wake(radio_app_t *app, uint32_t now_ms);

const char *radio_app_menu_label(radio_menu_item_t item);
void radio_app_menu_value(const radio_app_t *app, radio_menu_item_t item,
                          char *buffer, size_t buffer_size);
bool radio_app_menu_is_action(radio_menu_item_t item);
void radio_app_menu_adjust(radio_app_t *app, radio_menu_item_t item,
                           int16_t delta, uint32_t now_ms);
void radio_app_menu_activate(radio_app_t *app, radio_menu_item_t item,
                             uint32_t now_ms);

#endif /* RADIO_APP_H */

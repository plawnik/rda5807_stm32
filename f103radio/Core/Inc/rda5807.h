#ifndef RDA5807_H
#define RDA5807_H

#include "app_config.h"
#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RDA5807_OK = 0,
  RDA5807_NOT_FOUND,
  RDA5807_IO_ERROR,
  RDA5807_INVALID_ARGUMENT
} rda5807_result_t;

typedef enum {
  RDA5807_OPERATION_IDLE = 0,
  RDA5807_OPERATION_TUNING,
  RDA5807_OPERATION_SEEKING_UP,
  RDA5807_OPERATION_SEEKING_DOWN
} rda5807_operation_t;

typedef struct {
  uint32_t frequency_khz;
  uint16_t rds_blocks[4];
  uint8_t rssi;
  uint8_t bler_a;
  uint8_t bler_b;
  bool stereo;
  bool station_valid;
  bool tuner_ready;
  bool rds_ready;
  bool rds_synchronized;
  bool seek_failed;
} rda5807_status_t;

typedef struct {
  I2C_HandleTypeDef *i2c;
  uint16_t registers[6]; /* 0x02 through 0x07. */
  rda5807_status_t status;
  rda5807_operation_t operation;
  uint8_t consecutive_errors;
  bool online;
} rda5807_t;

rda5807_result_t rda5807_init(rda5807_t *radio, I2C_HandleTypeDef *i2c,
                              const radio_settings_t *settings);
rda5807_result_t rda5807_apply_settings(
    rda5807_t *radio, const radio_settings_t *settings);
rda5807_result_t rda5807_tune(rda5807_t *radio,
                              const radio_settings_t *settings);
rda5807_result_t rda5807_seek(rda5807_t *radio,
                              const radio_settings_t *settings, bool upwards);
rda5807_result_t rda5807_poll(rda5807_t *radio,
                              const radio_settings_t *settings);

#endif /* RDA5807_H */

#include "rda5807.h"

#include "board_config.h"

#include <string.h>

#define RDA5807_SEQUENTIAL_ADDRESS (0x10U << 1)

#define REG02_DHIZ              (1U << 15)
#define REG02_DMUTE             (1U << 14)
#define REG02_MONO              (1U << 13)
#define REG02_BASS              (1U << 12)
#define REG02_SEEKUP            (1U << 9)
#define REG02_SEEK              (1U << 8)
#define REG02_SKMODE            (1U << 7)
#define REG02_RDS_EN            (1U << 3)
#define REG02_NEW_METHOD        (1U << 2)
#define REG02_SOFT_RESET        (1U << 1)
#define REG02_ENABLE            (1U << 0)

#define REG03_TUNE              (1U << 4)
#define REG04_RBDS              (1U << 13)
#define REG04_DE_50US           (1U << 11)
#define REG04_SOFTMUTE_EN       (1U << 9)
#define REG04_AFCD              (1U << 8)
#define REG05_INT_MODE          (1U << 15)
#define REG07_SOFTBLEND_EN      (1U << 1)
#define REG07_EAST_65MHZ        (1U << 9)

#define REG0A_RDSR              (1U << 15)
#define REG0A_STC               (1U << 14)
#define REG0A_SF                (1U << 13)
#define REG0A_RDSS              (1U << 12)
#define REG0A_ST                (1U << 10)
#define REG0A_READCHAN_MASK     0x03FFU

static uint16_t from_be16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static void build_registers(rda5807_t *radio,
                            const radio_settings_t *settings) {
  const uint32_t band_min = radio_band_min_khz(settings);
  const uint16_t spacing = radio_spacing_khz(settings);
  const uint16_t channel = (uint16_t)(
      (settings->frequency_khz - band_min) / spacing);

  radio->registers[0] = REG02_DHIZ | REG02_ENABLE;
  if (!settings->muted) radio->registers[0] |= REG02_DMUTE;
  if (settings->force_mono) radio->registers[0] |= REG02_MONO;
  if (settings->bass_boost) radio->registers[0] |= REG02_BASS;
  if (settings->rds_enabled) radio->registers[0] |= REG02_RDS_EN;
  if (settings->new_method_enabled) radio->registers[0] |= REG02_NEW_METHOD;
  if (settings->seek_stop_at_band) radio->registers[0] |= REG02_SKMODE;

  radio->registers[1] = (uint16_t)((channel & 0x03FFU) << 6);
  radio->registers[1] |= (uint16_t)((settings->band & 0x03U) << 2);
  radio->registers[1] |= (uint16_t)(settings->spacing & 0x03U);

  radio->registers[2] = 0U;
  if (settings->rbds_enabled) radio->registers[2] |= REG04_RBDS;
  if (settings->deemphasis_50us) radio->registers[2] |= REG04_DE_50US;
  if (settings->softmute_enabled) radio->registers[2] |= REG04_SOFTMUTE_EN;
  if (!settings->afc_enabled) radio->registers[2] |= REG04_AFCD;

  radio->registers[3] = REG05_INT_MODE;
  radio->registers[3] |= (uint16_t)((settings->seek_mode & 0x03U) << 13);
  radio->registers[3] |= (uint16_t)((settings->seek_threshold & 0x0FU) << 8);
  radio->registers[3] |= (uint16_t)((settings->lna_port & 0x03U) << 6);
  radio->registers[3] |= (uint16_t)((settings->lna_current & 0x03U) << 4);
  radio->registers[3] |= (uint16_t)(settings->volume & 0x0FU);

  /* Register 0x06 remains zero: I2S and reserved-register writes stay off. */
  radio->registers[4] = 0U;

  radio->registers[5] =
      (uint16_t)((settings->softblend_threshold & 0x1FU) << 10);
  radio->registers[5] |=
      (uint16_t)((settings->old_seek_threshold & 0x3FU) << 2);
  if (settings->softblend_enabled) {
    radio->registers[5] |= REG07_SOFTBLEND_EN;
  }
  if (settings->east_band_starts_at_65mhz) {
    radio->registers[5] |= REG07_EAST_65MHZ;
  }
}

static rda5807_result_t write_registers(rda5807_t *radio) {
  uint8_t data[12];
  HAL_StatusTypeDef result;

  for (uint8_t i = 0U; i < 6U; ++i) {
    data[i * 2U] = (uint8_t)(radio->registers[i] >> 8);
    data[i * 2U + 1U] = (uint8_t)radio->registers[i];
  }

  result = HAL_I2C_Master_Transmit(radio->i2c, RDA5807_SEQUENTIAL_ADDRESS,
                                   data, sizeof(data),
                                   RDA5807_I2C_TIMEOUT_MS);
  if (result != HAL_OK) {
    radio->online = false;
    return RDA5807_IO_ERROR;
  }
  radio->online = true;
  radio->consecutive_errors = 0U;
  return RDA5807_OK;
}

rda5807_result_t rda5807_init(rda5807_t *radio, I2C_HandleTypeDef *i2c,
                              const radio_settings_t *settings) {
  if (radio == NULL || i2c == NULL || settings == NULL) {
    return RDA5807_INVALID_ARGUMENT;
  }

  memset(radio, 0, sizeof(*radio));
  radio->i2c = i2c;

  if (HAL_I2C_IsDeviceReady(i2c, RDA5807_SEQUENTIAL_ADDRESS, 3U,
                            RDA5807_I2C_TIMEOUT_MS) != HAL_OK) {
    return RDA5807_NOT_FOUND;
  }

  build_registers(radio, settings);
  radio->registers[0] |= REG02_SOFT_RESET;
  if (write_registers(radio) != RDA5807_OK) {
    return RDA5807_IO_ERROR;
  }
  HAL_Delay(20U);

  build_registers(radio, settings);
  if (write_registers(radio) != RDA5807_OK) {
    return RDA5807_IO_ERROR;
  }
  HAL_Delay(30U);
  return rda5807_tune(radio, settings);
}

rda5807_result_t rda5807_apply_settings(
    rda5807_t *radio, const radio_settings_t *settings) {
  if (radio == NULL || settings == NULL) return RDA5807_INVALID_ARGUMENT;
  build_registers(radio, settings);
  radio->operation = RDA5807_OPERATION_IDLE;
  return write_registers(radio);
}

rda5807_result_t rda5807_tune(rda5807_t *radio,
                              const radio_settings_t *settings) {
  if (radio == NULL || settings == NULL) return RDA5807_INVALID_ARGUMENT;
  build_registers(radio, settings);
  radio->registers[1] |= REG03_TUNE;
  radio->operation = RDA5807_OPERATION_TUNING;
  return write_registers(radio);
}

rda5807_result_t rda5807_seek(rda5807_t *radio,
                              const radio_settings_t *settings, bool upwards) {
  if (radio == NULL || settings == NULL) return RDA5807_INVALID_ARGUMENT;
  build_registers(radio, settings);
  radio->registers[0] |= REG02_SEEK;
  if (upwards) radio->registers[0] |= REG02_SEEKUP;
  radio->operation = upwards ? RDA5807_OPERATION_SEEKING_UP
                             : RDA5807_OPERATION_SEEKING_DOWN;
  return write_registers(radio);
}

rda5807_result_t rda5807_poll(rda5807_t *radio,
                              const radio_settings_t *settings) {
  uint8_t data[12];
  uint16_t reg0a;
  uint16_t reg0b;

  if (radio == NULL || settings == NULL) return RDA5807_INVALID_ARGUMENT;
  if (HAL_I2C_Master_Receive(radio->i2c, RDA5807_SEQUENTIAL_ADDRESS, data,
                            sizeof(data), RDA5807_I2C_TIMEOUT_MS) != HAL_OK) {
    if (radio->consecutive_errors < UINT8_MAX) ++radio->consecutive_errors;
    if (radio->consecutive_errors >= 3U) radio->online = false;
    return RDA5807_IO_ERROR;
  }

  radio->online = true;
  radio->consecutive_errors = 0U;
  reg0a = from_be16(&data[0]);
  reg0b = from_be16(&data[2]);

  radio->status.rds_ready = (reg0a & REG0A_RDSR) != 0U;
  radio->status.seek_failed = (reg0a & REG0A_SF) != 0U;
  radio->status.rds_synchronized = (reg0a & REG0A_RDSS) != 0U;
  radio->status.stereo = (reg0a & REG0A_ST) != 0U;
  radio->status.rssi = (uint8_t)((reg0b >> 9) & 0x7FU);
  radio->status.station_valid = (reg0b & (1U << 8)) != 0U;
  radio->status.tuner_ready = (reg0b & (1U << 7)) != 0U;
  radio->status.bler_a = (uint8_t)((reg0b >> 2) & 0x03U);
  radio->status.bler_b = (uint8_t)(reg0b & 0x03U);
  radio->status.frequency_khz = radio_band_min_khz(settings) +
      ((uint32_t)(reg0a & REG0A_READCHAN_MASK) * radio_spacing_khz(settings));
  for (uint8_t i = 0U; i < 4U; ++i) {
    radio->status.rds_blocks[i] = from_be16(&data[4U + (i * 2U)]);
  }

  if ((reg0a & REG0A_STC) != 0U &&
      radio->operation != RDA5807_OPERATION_IDLE) {
    radio->registers[0] &= (uint16_t)~REG02_SEEK;
    radio->registers[1] &= (uint16_t)~REG03_TUNE;
    radio->operation = RDA5807_OPERATION_IDLE;
    return write_registers(radio);
  }
  return RDA5807_OK;
}

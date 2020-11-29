/*
 * rda5807.c
 *
 *  Created on: Oct 11, 2020
 *      Author: pawel
 */

#include "rda5807.h"
#include "main.h"
#include "uart_debug.h"

I2C_HandleTypeDef rdahi2c;
rda5807_config_t rda5807_config;
rda5807_status_t rda5807_status;

extern I2C_HandleTypeDef hi2c2; //TODO: wyjebac

uint16_t frequency;
uint16_t band;
uint8_t spacing = SPACING_100KHZ;
uint8_t volume;

// TODO: add defines for all initial values
// ex: #define DEFAULT_SEEKTH 0b1000
int8_t rda5807_init(I2C_HandleTypeDef *i2c_h) {
  // check is RDA I2C working
  if (rda5807_check_is_connected(i2c_h) == RDA5807_NOT_FOUND) {
    return RDA5807_NOT_FOUND;
  }
  rdahi2c = *i2c_h;
  // software reset chip
  rda5807_write_register(0x02, 2);
  HAL_Delay(100); // datasheet say nothing about power up time

  // set init values of registers
  // register 0x02
  rda5807_config.reg02.refined.DHIZ = 1;
  rda5807_config.reg02.refined.DMUTE = 1;
  rda5807_config.reg02.refined.ENABLE = 1;
  rda5807_config.reg02.refined.BASS = 1;  // TODO: read from eeprom
  rda5807_config.reg02.refined.NEW_METHOD = 1; //TODO: read from eeprom
  rda5807_config.reg02.refined.RDS_EN = 1;
  // TODO: MONO mode read from eeprom

  // register 0x03
  //rda5807_config.reg03.raw = 0x4FC0;
  //rda5807_config.reg03.raw = (23<<4);
  //TODO: read spacing from eeprom
  //TODO: set last frequency from eeprom

  // register 0x04
  rda5807_config.reg04.raw = 0x0400;
  // TODO: read de-emphasis mode from eeprom

  // register 0x05
  rda5807_config.reg05.refined.INT_MODE = 1;
  rda5807_config.reg05.refined.SEEKTH = 8; //TODO: read from eeprom
  rda5807_config.reg05.refined.LNA_PORT_SEL = 2; //TODO: read from eeprom
  //TODO: add LNA_ICSEL set from eeprom
  rda5807_config.reg05.refined.VOLUME = 15; //TODO: add read last volume from eeprom

  // register 0x06
  rda5807_config.reg06.raw = 0x0000;

  // register 0x07
  rda5807_config.reg07.refined.TH_SOFRBLEND = 16;
  rda5807_config.reg07.refined.SOFTBLEND_EN = 1;
  rda5807_config.reg07.refined.MODE_50_65 = 1;

  // ragister 0x08
  rda5807_config.reg08.raw = 0x0000;

  // wtite registers values
  rda5807_write_register(0x02, rda5807_config.reg02.raw);
  rda5807_write_register(0x04, rda5807_config.reg04.raw);
  rda5807_write_register(0x05, rda5807_config.reg05.raw);

  // set default freq TODO: read last freq from eeprom
  rda5807_set_frequency(106100);
  HAL_Delay(10); // nothing about delay after init in datasheet

  return RDA5807_OK;
}

int8_t rda5807_check_is_connected(I2C_HandleTypeDef *i2c) {
  uint8_t err = 0;
  err = HAL_I2C_IsDeviceReady(i2c, 0x10 << 1, 2, 500);
  return err ? RDA5807_NOT_FOUND : RDA5807_OK;
}

/* Function writing register value to specific address.
 * Undocumented in datasheet.
 */
int8_t rda5807_write_register(uint8_t reg, uint16_t val) {
  // check register addres is valid
  if (reg < 0x02 || reg > 0x08)
    return RDA5807_WRITE_ERROR;
  // prepare send buffer
  uint8_t data[3];
  data[0] = reg;
  data[1] = val >> 8;
  data[2] = (uint8_t) val & 0xFF;

  // send data to rda5807
  HAL_StatusTypeDef err;
  err = HAL_I2C_Master_Transmit(&hi2c2, 0x11 << 1, data, 3, 3000); //TODO: change to rdahic
  if (err != HAL_OK)
    return RDA5807_WRITE_ERROR;
  return RDA5807_OK;
}

/* Function reading status registers 0x0A-0x0F */
void rda5807_read_status(void) {
  // read 6 words from i2c bus
  uint8_t buf[12];
  HAL_I2C_Master_Receive(&rdahi2c, 0x10 << 1, buf, 12, 1000);
  // switch bytes in words
  for (int i = 0; i < 6; i++) {
    uint8_t _c = buf[2 * i];
    buf[2 * i] = buf[2 * i + 1];
    buf[2 * i + 1] = _c;
  }
  // copy data to status structure
  memcpy(&rda5807_status, buf, sizeof(buf));
}

/* Function reading all registers - config+status 0x00-0x0F */
void rda5807_read_status_ex(void) {
  uint8_t data[256];
  memset(data, 0, 256);
  HAL_I2C_Mem_Read(&hi2c2, 0x11 << 1, 0, 2, data, 32, 1000);
  for (int i = 0; i < 16; i++) {
    dbg("reg %d = %x\n\r", i, ((data[i * 2] << 8) + data[i * 2 + 1]));
  }
}

/*
 Channel Select (0x03H_bit<9:0>) (0-1023)
 BAND = 0
 Frequency = Channel Spacing (kHz) x CHAN+ 87.0 MHz
 BAND = 1 or 2
 Frequency = Channel Spacing (kHz) x CHAN + 76.0 MHz
 BAND = 3
 Frequency = Channel Spacing (kHz) x CHAN + 65.0 MHz

 Band select (0x03H_bit<3:2>)


 65M_50M MODE (0x07H_bit<9>)
 Valid when band[1:0] = 2’b11 (0x03H_bit<3:2>)
 1 = 65~76 MHz;
 0 = 50~76 MHz.
 */
void rda5807_set_frequency(int freq) {
  /* US/EUROPE band 87-108MHz*/
  if (freq >= 87000) {

    freq -= 87000;
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      freq /= 100;
      break;
    case SPACING_200KHZ:
      freq /= 200;
      break;
    case SPACING_25KHZ:
      freq /= 25;
      break;
    case SPACING_50KHZ:
      freq /= 50;
      break;
    default:
      break;
    }
    // check is it valid 10bit value
    if (freq > 1023) {
      dbg("Set channel value out of range! Need to change spacing!\n\r");
      return;
    }
    // write registers
    rda5807_config.reg03.refined.BAND = BAND_US_EUROPE;
    rda5807_config.reg03.refined.CHAN = freq;
    rda5807_config.reg03.refined.TUNE = 1;
    rda5807_write_register(0x03, rda5807_config.reg03.raw);
    return;
  }
  /* JAPAN band 76-87MHz */
  if (freq >= 76000) {
    freq -= 76000;
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      freq /= 100;
      break;
    case SPACING_200KHZ:
      freq /= 200;
      break;
    case SPACING_25KHZ:
      freq /= 25;
      break;
    case SPACING_50KHZ:
      freq /= 50;
      break;
    default:
      break;
    }
    // check is it valid 10bit value
    if (freq > 1023) {
      dbg("Set channel value out of range! Need to change spacing!\n\r");
      return;
    }
    // write registers
    rda5807_config.reg03.refined.BAND = BAND_JAPAN;
    rda5807_config.reg03.refined.CHAN = freq;
    rda5807_config.reg03.refined.TUNE = 1;
    rda5807_write_register(0x03, rda5807_config.reg03.raw);
    return;
  }
  /* EAST EU upper band 50-76MHz */
  if (freq >= 65000) {
    freq -= 65000;
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      freq /= 100;
      break;
    case SPACING_200KHZ:
      freq /= 200;
      break;
    case SPACING_25KHZ:
      freq /= 25;
      break;
    case SPACING_50KHZ:
      freq /= 50;
      break;
    default:
      break;
    }
    // check is it valid 10bit value
    if (freq > 1023) {
      dbg("Set channel value out of range! Need to change spacing!\n\r");
      return;
    }
    // write registers
    rda5807_config.reg07.refined.MODE_50_65 = 1;
    rda5807_write_register(0x07, rda5807_config.reg07.raw);
    rda5807_config.reg03.refined.BAND = BAND_EAST_EUROPE;
    rda5807_config.reg03.refined.CHAN = freq;
    rda5807_config.reg03.refined.TUNE = 1;
    rda5807_write_register(0x03, rda5807_config.reg03.raw);
    return;
  }
  /* EAST EU lower band 50-65MHz */
  if (freq >= 50000) {
    freq -= 50000;
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      freq /= 100;
      break;
    case SPACING_200KHZ:
      freq /= 200;
      break;
    case SPACING_25KHZ:
      freq /= 25;
      break;
    case SPACING_50KHZ:
      freq /= 50;
      break;
    default:
      break;
    }
    // check is it valid 10bit value
    if (freq > 1023) {
      dbg("Set channel value out of range! Need to change spacing!\n\r");
      return;
    }
    // write registers
    rda5807_config.reg07.refined.MODE_50_65 = 0;
    rda5807_write_register(0x07, rda5807_config.reg07.raw);
    rda5807_config.reg03.refined.BAND = BAND_EAST_EUROPE;
    rda5807_config.reg03.refined.CHAN = freq;
    rda5807_config.reg03.refined.TUNE = 1;
    rda5807_write_register(0x03, rda5807_config.reg03.raw);
    return;
  }
  return;
}

int rda5807_get_frequency(void) {
  /* band US/EUROPE - 87-108MHz */
  if (rda5807_config.reg03.refined.BAND == BAND_US_EUROPE) {
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 100000 + 87000000;
      break;
    case SPACING_50KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 50000 + 87000000;
      break;
    case SPACING_200KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 200000 + 87000000;
      break;
    case SPACING_25KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 25000 + 87000000;
      break;
    default:
      return 0;
    }
  }
  /* band JAPAN - 76-91MHz or WORLD WIDE - 76-108MHz  */
  else if (rda5807_config.reg03.refined.BAND == BAND_JAPAN
      || rda5807_config.reg03.refined.BAND == BAND_WORLD_WIDE) {
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 100000 + 76000000;
      break;
    case SPACING_50KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 50000 + 76000000;
      break;
    case SPACING_200KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 200000 + 76000000;
      break;
    case SPACING_25KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 25000 + 76000000;
      break;
    default:
      return 0;
    }
  }
  /* band EAST EUROPE 65-76MHz */
  else if ((rda5807_config.reg03.refined.BAND == BAND_EAST_EUROPE)
      && (rda5807_config.reg07.refined.MODE_50_65 == 1)) {
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 100000 + 65000000;
      break;
    case SPACING_50KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 50000 + 65000000;
      break;
    case SPACING_200KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 200000 + 65000000;
      break;
    case SPACING_25KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 25000 + 65000000;
      break;
    default:
      return 0;
    }
  }
  /* band EAST EUROPE 50-76MHz */
  else if ((rda5807_config.reg03.refined.BAND == BAND_EAST_EUROPE)
      && (rda5807_config.reg07.refined.MODE_50_65 == 0)) {
    switch (rda5807_config.reg03.refined.SPACE) {
    case SPACING_100KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 100000 + 50000000;
      break;
    case SPACING_50KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 50000 + 50000000;
      break;
    case SPACING_200KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 200000 + 50000000;
      break;
    case SPACING_25KHZ:
      return rda5807_status.reg0a.refined.READCHAN * 25000 + 50000000;
      break;
    default:
      return 0;
    }
  }
  return 0;
}

int rda5807_get_rssi(void) {
//FIXME: idk how its scale
  return (-127 + rda5807_status.reg0b.refined.RSSI);
}

int rda5807_get_stereo(void) {
  return rda5807_status.reg0a.refined.ST;
}

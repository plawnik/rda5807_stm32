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



int8_t rda5807_init(I2C_HandleTypeDef *i2c_h) {
  // check is RDA I2C working
  if (rda5807_check_is_connected(i2c_h) == RDA5807_NOT_FOUND) {
    return RDA5807_NOT_FOUND;
  }
  rdahi2c = *i2c_h;
  dbg("1 = %x\n\r",rda5807_config.reg00.raw);
  dbg("2 = %x\n\r",rda5807_config.reg01.raw);
  dbg("3 = %x\n\r",rda5807_config.reg02.raw);
  dbg("4 = %x\n\r",rda5807_config.reg03.raw);
  dbg("5 = %x\n\r",rda5807_config.reg04.raw);

  // set init values of registers
  rda5807_config.reg02.refined.MONO = 1;
  rda5807_config.reg02.refined.DMUTE = 1;
  rda5807_config.reg02.refined.DHIZ = 1;
  rda5807_config.reg02.refined.ENABLE = 1;

  rda5807_config.reg05.refined.INT_MODE = 0;
  rda5807_config.reg05.refined.LNA_PORT_SEL = 2;
  rda5807_config.reg05.refined.LNA_ICSEL_BIT = 0;
  rda5807_config.reg05.refined.SEEKTH = 8;
  rda5807_config.reg05.refined.VOLUME = 0b1011;
  //rda5807_config.reg00.refined.CHIPID = 0x04;
  // TODO:
  //rda5807_write_register(0x02, rda5807_config.reg02.raw);
  //rda5807_write_register(0x02, rda5807_config.reg05.raw);

  rda5807_write_register(0x02, 0b1111000000000001);
  rda5807_write_register(0x05, 0b0000100010111111);

  return RDA5807_OK;
}

int8_t rda5807_check_is_connected(I2C_HandleTypeDef *i2c) {
  uint8_t err = 0;
  err = HAL_I2C_IsDeviceReady(i2c, 0x10 << 1, 2, 50);
  return err ? RDA5807_NOT_FOUND : RDA5807_OK;
}

int8_t rda5807_write_register(uint8_t reg, uint16_t val) {
  // check register addres is valid
  if (reg < 0x02 || reg > 0x08)
    return RDA5807_WRITE_ERROR;
  // prepare send buffer
  uint8_t data[3];
  data[0] = reg;
  data[1] = val >> 8;
  data[2] = (uint8_t)val & 0xFF;


  dbg("chip id = %x\n\r",rda5807_config.reg00.raw);
  dbg("sizeof s truct = %d\n\r",sizeof(rda5807_config.reg02));
  dbg("r = %x\n\r", val);
  dbg("1=%x 2=%x=3%x\n\r", data[0],data[1],data[2]);
  //while(1);
  // send data to rda5807
  HAL_StatusTypeDef err;
  err = HAL_I2C_Master_Transmit(&rdahi2c, RDA5807_DIRECT_WRITE_REGISTER << 1,
                                 data, 3, 300);
  if(err!=HAL_OK)
    return RDA5807_WRITE_ERROR;
  return RDA5807_OK;

}

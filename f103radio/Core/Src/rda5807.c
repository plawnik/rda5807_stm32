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

uint16_t frequency;
uint16_t band;
uint8_t spacing = 1;
uint8_t volume;

int8_t rda5807_init(I2C_HandleTypeDef *i2c_h) {
	// check is RDA I2C working
	if (rda5807_check_is_connected(i2c_h) == RDA5807_NOT_FOUND) {
		return RDA5807_NOT_FOUND;
	}
	rdahi2c = *i2c_h;

	// set init values of registers
	rda5807_config.reg02.refined.DMUTE = 1;
	rda5807_config.reg02.refined.DHIZ = 1;
	rda5807_config.reg02.refined.ENABLE = 1;
	rda5807_config.reg02.refined.BASS = 1;
	//rda5807_config.reg02.refined.RDS_EN = 1;

	rda5807_config.reg04.refined.RDS_FIFO_CLR = 1;

	rda5807_config.reg05.refined.INT_MODE = 0;
	rda5807_config.reg05.refined.LNA_PORT_SEL = 2;
	rda5807_config.reg05.refined.LNA_ICSEL_BIT = 0;
	rda5807_config.reg05.refined.SEEKTH = 8;
	rda5807_config.reg05.refined.VOLUME = 0b0001;

	//rda5807_config.reg03.refined.CHAN = 0x15F;

	// TODO:
	//rda5807_write_register(0x02, 2); // software reset chip;
	rda5807_write_register(0x03, rda5807_config.reg03.raw);
	rda5807_write_register(0x02, rda5807_config.reg02.raw);
	rda5807_write_register(0x05, rda5807_config.reg05.raw);
	rda5807_write_register(0x02, rda5807_config.reg02.raw);

	HAL_Delay(500);

	//rda5807_write_register(0x02, 0b1111000000000001);
	//rda5807_write_register(0x05, 0b0000100010111111);

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
	data[2] = (uint8_t) val & 0xFF;

	// send data to rda5807
	HAL_StatusTypeDef err;
	err = HAL_I2C_Master_Transmit(&rdahi2c, 0x11 << 1, data, 3, 300);
	if (err != HAL_OK)
		return RDA5807_WRITE_ERROR;
	return RDA5807_OK;
}

void rda5807_read_status(void) {
	HAL_I2C_Master_Receive(&rdahi2c, 0x10<<1, (uint8_t*)&rda5807_status, 12, 10);
	dbg("a %x\n\r",rda5807_status.reg0a.raw);
	dbg("b %x\n\r",rda5807_status.reg0b.raw);
	dbg("c %x\n\r",rda5807_status.reg0c.RDSA);
	dbg("d %x\n\r",rda5807_status.reg0d.RDSB);
}

void rda5807_read_status_ex(void) {
	uint8_t data[256];
	memset(data, 0, 256);
	//int error = HAL_I2C_Master_Receive(&hi2c2, 0x11<<1, data,64, 10);
	HAL_I2C_Mem_Read(&rdahi2c, 0x11 << 1, 0, 2, data, 32, 10);

	for (int i = 0; i < 16; i++) {
		dbg("reg %d = %x\n\r", i, ((data[i * 2] << 8) + data[i * 2 + 1]));
	}

}

/*
 *
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
	if (freq < 65000) {
		//config 50MHz-65MHz mode
		rda5807_config.reg03.refined.BAND = 0b11;
		rda5807_config.reg07.refined.MODE_50_60 = 0;

	} else if (freq < 76000) {
		//config 65MHz-76MHz mode CCIR
		rda5807_config.reg03.refined.BAND = 0b11;
		rda5807_config.reg07.refined.MODE_50_60 = 1;
	} else if (freq < 87000) {
		//config 76MHz-87MHz mode
	} else {
		//config 87MHz-108MHz mode OIRT
	}

}

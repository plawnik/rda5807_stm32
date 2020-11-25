/*
 * lcd.h
 *
 *  Created on: 24 lis 2020
 *      Author: miszczo
 */

#pragma once

#include "i2c.h"


#define LED_PIN 3
#define RW_PIN 1
#define RS_PIN 0
#define E_PIN 2

#define LCD_I2C_HANDLER &hi2c1
#define LCD_I2C_ADDRESS 0x3F


typedef struct {
	uint8_t addr;
	uint8_t iostate;
	I2C_HandleTypeDef *hi2c;
}lcd_t;



void lcd_init(lcd_t *lcd,uint8_t addr,I2C_HandleTypeDef *hi2c);
void lcd_clear(lcd_t *lcd);
void lcd_pos(lcd_t *lcd, int x, int y);

void lcd_char(lcd_t *lcd, char c);
void lcd_string(lcd_t *lcd, char *string);
void lcd_int(lcd_t *lcd,int integer);
void lcd_hex(lcd_t *lcd,int hexadecimal);
void lcd_bin(lcd_t *lcd,int binary);


void lcd_rewrite(lcd_t *_lcd, char *text);
void lcd_clr_write_at(lcd_t *_lcd, char *text, int pos);
void lcd_write_at(lcd_t *_lcd, char *text, int pos);



/*
 * lcd.c
 *
 *  Created on: 13.08.2019
 *      Author: Win7
 */


#include "lcd.h"
#include <stdlib.h>
#include "uart_debug.h"


// local functions
// pin manipulation
static void lcd_set_e(lcd_t *l);
static void lcd_reset_e(lcd_t *l);
static void lcd_set_rw(lcd_t *l);
static void lcd_reset_rw(lcd_t *l);
static void lcd_set_rs(lcd_t *l);
static void lcd_reset_rs(lcd_t *l);
static void lcd_set_led(lcd_t *l);
static void lcd_reset_led(lcd_t *l);
// delays
static void _delay(uint32_t cnt);
static void lcd_delay();
// data transfer
static void lcd_pcf8574_write(lcd_t *s, uint8_t data);
static void lcd_nibble(lcd_t *l, uint8_t nibble);
static void  lcd_byte(lcd_t *l , uint8_t byte, uint8_t dc);


// E
static void lcd_set_e(lcd_t *l){
	l->iostate |= (1<<E_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
static void lcd_reset_e(lcd_t *l){
	l->iostate &=~(1<<E_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
// RW
static void lcd_set_rw(lcd_t *l){
	l->iostate |= (1<<RW_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
static void lcd_reset_rw(lcd_t *l){
	l->iostate &=~(1<<RW_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
// RS
static void lcd_set_rs(lcd_t *l){
	l->iostate |= (1<<RS_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
static void lcd_reset_rs(lcd_t *l){
	l->iostate &=~(1<<RS_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
// LED
static void lcd_set_led(lcd_t *l){
	l->iostate |= (1<<LED_PIN);
	lcd_pcf8574_write(l,l->iostate);
}
static void lcd_reset_led(lcd_t *l){
	l->iostate &=~(1<<LED_PIN);
	lcd_pcf8574_write(l,l->iostate);
}



void _delay(uint32_t cnt){
	while(cnt){
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		cnt--;
	}
}


void lcd_delay(){
	//_delay(10000); // ~1,2ms
	_delay(10000); // 0,12ms
}



void lcd_pcf8574_write(lcd_t *s, uint8_t data){
	s->iostate=data;
	uint8_t error=0;
	//vTaskSuspendAll();
	error = HAL_I2C_Master_Transmit(s->hi2c, (uint16_t)s->addr, &s->iostate, 1, 100);
	//xTaskResumeAll();
	(void)error;
	if(error!=0) dbg("eeror %d",error);

}


void lcd_nibble(lcd_t *l, uint8_t nibble){
	nibble=nibble<<4;
	lcd_set_e(l);
	lcd_delay();
	l->iostate&=0x0F;
	l->iostate|=nibble;
	lcd_pcf8574_write(l,l->iostate);
	lcd_delay();
	lcd_reset_e(l);
}

static void lcd_byte(lcd_t *l , uint8_t byte, uint8_t dc){
	if(dc==0){
		lcd_reset_rs(l);
	}
	else
		lcd_set_rs(l);

	lcd_nibble(l,byte>>4);
	lcd_nibble(l,byte);
}




void lcd_init(lcd_t *lcd,uint8_t addr,I2C_HandleTypeDef *hi2c){
	// fix warnings unused definitions
	(void)&lcd_set_rw;
	(void)&lcd_reset_rw;
	(void)&lcd_reset_led;


	// init pcf struct
	lcd->addr=addr;
	lcd->hi2c=hi2c;
	lcd->iostate=0;

	lcd_set_led(lcd);

	HAL_Delay(15); //wait to power up

	lcd_nibble(lcd,0x03);
	HAL_Delay(45);
	lcd_nibble(lcd,0x03);
	HAL_Delay(45);
	lcd_nibble(lcd,0x03);
	HAL_Delay(45);
	lcd_nibble(lcd,0x02);
	HAL_Delay(100);

	lcd_byte(lcd,0b00101000,0);
	lcd_byte(lcd,0b00001100,0);
	lcd_byte(lcd,0b00000110,0);


	lcd_byte(lcd,0x01,0); // clear lcd
}

void lcd_clear(lcd_t *lcd){
	lcd_byte(lcd,0x01,0);
}

void lcd_char(lcd_t *lcd, char c){
	lcd_byte(lcd,c,1);
}

void lcd_string(lcd_t *lcd, char *string){
	while(*string) lcd_char(lcd,*string++);
}

void lcd_int(lcd_t *lcd,int integer){
	char buffer[20];
	itoa(integer , buffer , 10); // data , destination , system
	lcd_string(lcd,buffer);
}

void lcd_hex(lcd_t *lcd,int hexadecimal)
{
	char buffer[15];
	itoa(hexadecimal , buffer , 16); // data , destination , system
	lcd_string(lcd,buffer);
}

void lcd_bin(lcd_t *lcd,int binary){
	char buffer[35];
	itoa(binary , buffer , 2); // data , destination , system
	lcd_string(lcd,buffer);
}


// TODO: tests
void lcd_pos(lcd_t *lcd, int x, int y) {
	switch (y) {
	case 0:
		lcd_byte(lcd, x | 0x80 , 0);
		HAL_Delay(1);
		break;
	case 1:
		lcd_byte(lcd, (0x40 + x) | 0x80 , 0);
		HAL_Delay(1);
		break;
	}
}


//Rewrites whole y segment on LCD
void lcd_rewrite(lcd_t *_lcd, char *text) {
  lcd_pos(_lcd, 0, 0);
  lcd_string(_lcd,"                ");
  lcd_pos(_lcd, 0, 0);
  lcd_string(_lcd, text);
}

void lcd_clr_write_at(lcd_t *_lcd, char *text, int pos) {

  lcd_pos(_lcd, 0, 0);
  lcd_string(_lcd,"                ");
  lcd_pos(_lcd, pos, 0);
  lcd_string(_lcd, text);


}

void lcd_write_at(lcd_t *_lcd, char *text, int pos) {

  lcd_pos(_lcd, pos, 0);
  lcd_string(_lcd, text);

}

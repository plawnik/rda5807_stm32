/*
 * uart_debug.c
 *
 *  Created on: 31.10.2018
 *      Author: Win7
 */

#include "uart_debug.h"
#include "main.h"


UART_HandleTypeDef dbg_uart_handler;

void uart_dbg_init(UART_HandleTypeDef* uart_handler){
	dbg_uart_handler = *uart_handler;
	clear_console();
	hide_cursor_console();
}


void vprint(const char *fmt, va_list argp)
{
    char string[100];
    if(0 < vsprintf(string,fmt,argp)) // build string
    {
        HAL_UART_Transmit(&dbg_uart_handler, (uint8_t*)string, strlen(string),10); // send message via UART
    }
}

void dbg(const char *fmt, ...) // custom printf() function
{
    va_list argp;
    va_start(argp, fmt);
    vprint(fmt, argp);
    va_end(argp);
}

void clear_console(void){
  dbg("\033[2J\033[H");// clear console window
}


void home_console(void){
  dbg("\033[H");// clear console window
}

void hide_cursor_console(void){
	dbg("\e[?25l");
}

void show_cursor_console(void){
	dbg("\e[?25h");
}

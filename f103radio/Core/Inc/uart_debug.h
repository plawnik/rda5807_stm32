/*
 * uart_debug.h
 *
 *  Created on: 31.10.2018
 *      Author: Win7
 */

#ifndef UART_DEBUG_H_
#define UART_DEBUG_H_
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "main.h"


void uart_dbg_init(UART_HandleTypeDef* uart_handler);
void vprint(const char *fmt, va_list argp);
void dbg(const char *fmt, ...); // custom printf() function
void clear_console(void);

void home_console(void);
void hide_cursor_console(void);
void show_cursor_console(void);


#endif /* UART_DEBUG_H_ */

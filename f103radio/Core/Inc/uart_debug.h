#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include "stm32f1xx_hal.h"

#include <stddef.h>

void uart_debug_init(UART_HandleTypeDef *uart);
void uart_debug_write(const char *text);
void uart_debug_write_n(const char *data, size_t length);
void uart_debug_printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

#endif /* UART_DEBUG_H */

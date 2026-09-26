#include "uart_debug.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *debug_uart;

/* Host tests and builds without USB keep this weak no-op. The firmware's CDC
 * interface provides the strong implementation. */
__attribute__((weak)) bool usb_console_write_n(const char *data,
                                                size_t length) {
  (void)data;
  (void)length;
  return false;
}

void uart_debug_init(UART_HandleTypeDef *uart) {
  debug_uart = uart;
}

void uart_debug_write_n(const char *data, size_t length) {
  if (data == NULL || length == 0U) return;
  if (debug_uart != NULL) {
    HAL_UART_Transmit(debug_uart, (const uint8_t *)data, (uint16_t)length,
                      250U);
  }
  (void)usb_console_write_n(data, length);
}

void uart_debug_write(const char *text) {
  if (text != NULL) uart_debug_write_n(text, strlen(text));
}

void uart_debug_printf(const char *format, ...) {
  char buffer[256];
  va_list arguments;
  int length;
  if (format == NULL) return;
  va_start(arguments, format);
  length = vsnprintf(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  if (length <= 0) return;
  if ((size_t)length >= sizeof(buffer)) length = (int)sizeof(buffer) - 1;
  uart_debug_write_n(buffer, (size_t)length);
}

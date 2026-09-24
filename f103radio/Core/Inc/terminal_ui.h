#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include "radio_app.h"
#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#define TERMINAL_RX_BUFFER_SIZE 128U

typedef struct {
  UART_HandleTypeDef *uart;
  volatile uint8_t rx_buffer[TERMINAL_RX_BUFFER_SIZE];
  volatile uint8_t rx_head;
  volatile uint8_t rx_tail;
  uint8_t rx_byte;
  uint8_t parser_state;
  uint8_t frequency_digit;
  radio_menu_item_t selected;
  uint32_t parser_changed_ms;
  uint32_t last_render_ms;
  uint32_t rendered_revision;
  bool editing;
  bool force_render;
} terminal_ui_t;

void terminal_ui_init(terminal_ui_t *ui, UART_HandleTypeDef *uart,
                      uint32_t now_ms);
void terminal_ui_process(terminal_ui_t *ui, radio_app_t *app,
                         uint32_t now_ms);
void terminal_ui_render(terminal_ui_t *ui, const radio_app_t *app,
                        uint32_t now_ms);
void terminal_ui_uart_rx_complete(UART_HandleTypeDef *uart);
void terminal_ui_uart_error(UART_HandleTypeDef *uart);

#endif /* TERMINAL_UI_H */

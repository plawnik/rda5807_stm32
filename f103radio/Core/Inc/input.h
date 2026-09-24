#ifndef INPUT_H
#define INPUT_H

#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int16_t rotation;
  bool click;
  bool long_press;
} input_event_t;

typedef struct {
  TIM_HandleTypeDef *encoder_timer;
  uint32_t raw_changed_at;
  uint32_t pressed_at;
  bool raw_pressed;
  bool stable_pressed;
  bool long_press_reported;
} input_t;

void input_init(input_t *input, TIM_HandleTypeDef *encoder_timer,
                uint32_t now_ms);
input_event_t input_poll(input_t *input, uint32_t now_ms);

#endif /* INPUT_H */

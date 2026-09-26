#ifndef INPUT_H
#define INPUT_H

#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int16_t rotation;
  bool click;
  bool long_press;
  bool left;
  bool right;
  bool ok;
  bool ok_long_press;
} input_event_t;

typedef struct {
  uint32_t raw_changed_at;
  bool raw_pressed;
  bool stable_pressed;
} input_button_state_t;

typedef struct {
  TIM_HandleTypeDef *encoder_timer;
  uint32_t raw_changed_at;
  uint32_t pressed_at;
  bool raw_pressed;
  bool stable_pressed;
  bool long_press_reported;
  bool ignore_encoder_release;
  uint32_t ok_pressed_at;
  bool ok_long_press_reported;
  bool ignore_ok_release;
  input_button_state_t left_button;
  input_button_state_t right_button;
  input_button_state_t ok_button;
} input_t;

void input_init(input_t *input, TIM_HandleTypeDef *encoder_timer,
                uint32_t now_ms);
input_event_t input_poll(input_t *input, uint32_t now_ms);
void input_resync(input_t *input, uint32_t now_ms);

#endif /* INPUT_H */

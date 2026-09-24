#include "input.h"

#include "board_config.h"

#include <string.h>

#define BUTTON_DEBOUNCE_MS 25U
#define BUTTON_LONG_PRESS_MS 750U

static bool button_is_pressed(void) {
  return HAL_GPIO_ReadPin(ENCODER_BUTTON_GPIO_Port, ENCODER_BUTTON_Pin) ==
         ENCODER_BUTTON_ACTIVE_STATE;
}

void input_init(input_t *input, TIM_HandleTypeDef *encoder_timer,
                uint32_t now_ms) {
  if (input == NULL || encoder_timer == NULL) return;
  memset(input, 0, sizeof(*input));
  input->encoder_timer = encoder_timer;
  input->raw_pressed = button_is_pressed();
  input->stable_pressed = input->raw_pressed;
  input->raw_changed_at = now_ms;
  input->pressed_at = now_ms;
  __HAL_TIM_SET_COUNTER(encoder_timer, 0U);
  HAL_TIM_Encoder_Start(encoder_timer, TIM_CHANNEL_ALL);
}

input_event_t input_poll(input_t *input, uint32_t now_ms) {
  input_event_t event = {0};
  bool current;
  int16_t counter;
  int16_t steps;

  if (input == NULL || input->encoder_timer == NULL) return event;

  counter = (int16_t)__HAL_TIM_GET_COUNTER(input->encoder_timer);
  steps = (int16_t)(counter / ENCODER_COUNTS_PER_DETENT);
  if (steps != 0) {
    event.rotation = (int16_t)(steps * ENCODER_DIRECTION);
    counter = (int16_t)(counter - steps * ENCODER_COUNTS_PER_DETENT);
    __HAL_TIM_SET_COUNTER(input->encoder_timer, (uint16_t)counter);
  }

  current = button_is_pressed();
  if (current != input->raw_pressed) {
    input->raw_pressed = current;
    input->raw_changed_at = now_ms;
  }

  if (current != input->stable_pressed &&
      (uint32_t)(now_ms - input->raw_changed_at) >= BUTTON_DEBOUNCE_MS) {
    input->stable_pressed = current;
    if (current) {
      input->pressed_at = now_ms;
      input->long_press_reported = false;
    } else if (!input->long_press_reported) {
      event.click = true;
    }
  }

  if (input->stable_pressed && !input->long_press_reported &&
      (uint32_t)(now_ms - input->pressed_at) >= BUTTON_LONG_PRESS_MS) {
    input->long_press_reported = true;
    event.long_press = true;
  }
  return event;
}

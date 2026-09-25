#include "input.h"

#include "board_config.h"

#include <string.h>

#define BUTTON_DEBOUNCE_MS 25U
#define BUTTON_LONG_PRESS_MS 750U

static bool button_is_pressed(void) {
  return HAL_GPIO_ReadPin(ENCODER_BUTTON_GPIO_Port, ENCODER_BUTTON_Pin) ==
         ENCODER_BUTTON_ACTIVE_STATE;
}

static bool nav_button_is_pressed(GPIO_TypeDef *port, uint16_t pin) {
  return HAL_GPIO_ReadPin(port, pin) == NAV_BUTTON_ACTIVE_STATE;
}

static void nav_button_init(input_button_state_t *button, bool pressed,
                            uint32_t now_ms) {
  button->raw_pressed = pressed;
  button->stable_pressed = pressed;
  button->raw_changed_at = now_ms;
}

static bool nav_button_poll(input_button_state_t *button, bool current,
                            uint32_t now_ms) {
  if (current != button->raw_pressed) {
    button->raw_pressed = current;
    button->raw_changed_at = now_ms;
  }
  if (current != button->stable_pressed &&
      (uint32_t)(now_ms - button->raw_changed_at) >= BUTTON_DEBOUNCE_MS) {
    button->stable_pressed = current;
    return current;
  }
  return false;
}

void input_init(input_t *input, TIM_HandleTypeDef *encoder_timer,
                uint32_t now_ms) {
  if (input == NULL || encoder_timer == NULL) return;
  memset(input, 0, sizeof(*input));
  input->encoder_timer = encoder_timer;
  input->raw_pressed = button_is_pressed();
  input->stable_pressed = input->raw_pressed;
  input->ignore_encoder_release = input->raw_pressed;
  input->long_press_reported = input->raw_pressed;
  input->raw_changed_at = now_ms;
  input->pressed_at = now_ms;
  nav_button_init(&input->left_button,
                  nav_button_is_pressed(BUTTON_LEFT_GPIO_Port,
                                        BUTTON_LEFT_Pin), now_ms);
  nav_button_init(&input->right_button,
                  nav_button_is_pressed(BUTTON_RIGHT_GPIO_Port,
                                        BUTTON_RIGHT_Pin), now_ms);
  nav_button_init(&input->ok_button,
                  nav_button_is_pressed(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin),
                  now_ms);
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
    } else if (input->ignore_encoder_release) {
      input->ignore_encoder_release = false;
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
  event.left = nav_button_poll(
      &input->left_button,
      nav_button_is_pressed(BUTTON_LEFT_GPIO_Port, BUTTON_LEFT_Pin), now_ms);
  event.right = nav_button_poll(
      &input->right_button,
      nav_button_is_pressed(BUTTON_RIGHT_GPIO_Port, BUTTON_RIGHT_Pin), now_ms);
  event.ok = nav_button_poll(
      &input->ok_button,
      nav_button_is_pressed(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin), now_ms);
  return event;
}

void input_resync(input_t *input, uint32_t now_ms) {
  if (input == NULL || input->encoder_timer == NULL) return;
  input->raw_pressed = button_is_pressed();
  input->stable_pressed = input->raw_pressed;
  input->ignore_encoder_release = input->raw_pressed;
  input->long_press_reported = input->raw_pressed;
  input->raw_changed_at = now_ms;
  input->pressed_at = now_ms;
  nav_button_init(&input->left_button,
                  nav_button_is_pressed(BUTTON_LEFT_GPIO_Port,
                                        BUTTON_LEFT_Pin), now_ms);
  nav_button_init(&input->right_button,
                  nav_button_is_pressed(BUTTON_RIGHT_GPIO_Port,
                                        BUTTON_RIGHT_Pin), now_ms);
  nav_button_init(&input->ok_button,
                  nav_button_is_pressed(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin),
                  now_ms);
  __HAL_TIM_SET_COUNTER(input->encoder_timer, 0U);
}

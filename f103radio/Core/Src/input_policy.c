#include "input_policy.h"

#include "app_config.h"

input_event_t input_event_for_mode(input_event_t event, uint8_t input_mode) {
  if (input_mode == RADIO_INPUT_BUTTONS) {
    event.rotation = 0;
  } else {
    event.left = false;
    event.right = false;
  }
  return event;
}

bool input_event_requests_standby(input_event_t event) {
  return event.long_press || event.ok_long_press;
}

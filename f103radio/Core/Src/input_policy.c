#include "input_policy.h"

bool input_event_requests_standby(input_event_t event) {
  return event.long_press || event.ok_long_press;
}

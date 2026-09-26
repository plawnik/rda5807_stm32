#ifndef INPUT_POLICY_H
#define INPUT_POLICY_H

#include "input_event.h"

#include <stdint.h>

/* Select only the directional source. Both physical OK switches remain active
 * so either local interface can always enter the menu and control standby. */
input_event_t input_event_for_mode(input_event_t event, uint8_t input_mode);
bool input_event_requests_standby(input_event_t event);

#endif /* INPUT_POLICY_H */

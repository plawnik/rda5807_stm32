#ifndef INPUT_POLICY_H
#define INPUT_POLICY_H

#include "input_event.h"

/* Both the encoder and the three-button keypad are always active. */
bool input_event_requests_standby(input_event_t event);

#endif /* INPUT_POLICY_H */

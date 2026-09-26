#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

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

#endif /* INPUT_EVENT_H */

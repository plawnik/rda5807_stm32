#ifndef SPLASH_ANIMATION_H
#define SPLASH_ANIMATION_H

#include "pcd8544.h"

#include <stdbool.h>
#include <stdint.h>

void splash_play(pcd8544_t *lcd, bool startup);

/* Decode [run length, byte value] pairs directly into the 504-byte PCD8544
 * framebuffer.  It is used by assets generated with tools/convert_splash.py. */
bool splash_rle_decode(pcd8544_t *lcd, const uint8_t *data,
                       uint16_t data_length);

#endif /* SPLASH_ANIMATION_H */

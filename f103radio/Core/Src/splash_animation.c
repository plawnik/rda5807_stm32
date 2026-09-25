#include "splash_animation.h"

#include "splash_config.h"
#include "stm32f1xx_hal.h"

#include <string.h>

#define SPLASH_FRAME_COUNT 8U

static uint32_t random_state;

static uint32_t next_random(void) {
  if (random_state == 0U) {
    const uint32_t *uid = (const uint32_t *)0x1FFFF7E8UL;
    random_state = uid[0] ^ uid[1] ^ uid[2] ^ HAL_GetTick() ^ SysTick->VAL;
    if (random_state == 0U) random_state = 0x5807F103UL;
  }
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

static uint8_t choose_animation(uint32_t mask) {
  uint8_t count = 0U;
  uint8_t selected;
  for (uint8_t id = 0U; id < 32U; ++id) {
    if ((mask & SPLASH_ANIMATION_BIT(id)) != 0U) ++count;
  }
  selected = (uint8_t)(next_random() % count);
  for (uint8_t id = 0U; id < 32U; ++id) {
    if ((mask & SPLASH_ANIMATION_BIT(id)) == 0U) continue;
    if (selected-- == 0U) return id;
  }
  return SPLASH_ANIMATION_RADIO_DIAL;
}

static void draw_caption(pcd8544_t *lcd, bool startup) {
  const char *text = startup ? "START" : "STANDBY";
  const int16_t width = (int16_t)(strlen(text) * 6U);
  pcd8544_text(lcd, (int16_t)((PCD8544_WIDTH - width) / 2), 40, text, 1U,
                true);
}

static void draw_radio_dial(pcd8544_t *lcd, uint8_t frame, bool startup) {
  const uint8_t phase = startup ? frame : (uint8_t)(7U - frame);
  const int16_t marker = (int16_t)(9 + phase * 9U);
  pcd8544_text(lcd, 18, 2, "RDA5807", 1U, true);
  pcd8544_rect(lcd, 5, 12, 74, 24, true);
  pcd8544_line(lcd, 9, 26, 74, 26, true);
  for (int16_t x = 9; x <= 72; x += 9) {
    pcd8544_line(lcd, x, 23, x, 29, true);
  }
  pcd8544_fill_rect(lcd, marker, 18, 3, 14, true);
  pcd8544_text(lcd, 24, 30, startup ? "TUNING" : "OFF", 1U, true);
  draw_caption(lcd, startup);
}

static void draw_equalizer(pcd8544_t *lcd, uint8_t frame, bool startup) {
  pcd8544_text(lcd, 12, 1, "FM  RADIO", 1U, true);
  for (uint8_t bar = 0U; bar < 10U; ++bar) {
    uint8_t height = (uint8_t)(3U + ((bar * 5U + frame * 3U) % 21U));
    if (!startup) height = (uint8_t)((height * (7U - frame)) / 7U);
    pcd8544_rect(lcd, (int16_t)(7 + bar * 7U), 35, 4, 1, true);
    if (height != 0U) {
      pcd8544_fill_rect(lcd, (int16_t)(7 + bar * 7U),
                        (int16_t)(35 - height), 4, height, true);
    }
  }
  draw_caption(lcd, startup);
}

static void draw_antenna(pcd8544_t *lcd, uint8_t frame, bool startup) {
  const uint8_t phase = startup ? frame : (uint8_t)(7U - frame);
  pcd8544_line(lcd, 42, 14, 42, 35, true);
  pcd8544_line(lcd, 42, 14, 34, 35, true);
  pcd8544_line(lcd, 42, 14, 50, 35, true);
  pcd8544_line(lcd, 35, 28, 49, 28, true);
  pcd8544_fill_rect(lcd, 40, 12, 5, 5, true);
  for (uint8_t wave = 0U; wave < 3U; ++wave) {
    if (phase < wave * 2U) continue;
    const int16_t radius = (int16_t)(7 + wave * 6U);
    pcd8544_line(lcd, (int16_t)(42 - radius), (int16_t)(14 - wave),
                 (int16_t)(42 - radius + 3), (int16_t)(9 - wave), true);
    pcd8544_line(lcd, (int16_t)(42 + radius), (int16_t)(14 - wave),
                 (int16_t)(42 + radius - 3), (int16_t)(9 - wave), true);
  }
  pcd8544_text(lcd, 20, 1, "ON AIR", 1U, true);
  draw_caption(lcd, startup);
}

static void draw_power(pcd8544_t *lcd, uint8_t frame, bool startup) {
  const uint8_t phase = startup ? frame : (uint8_t)(7U - frame);
  pcd8544_rect(lcd, 13, 8, 58, 28, true);
  pcd8544_rect(lcd, 17, 12, 35, 16, true);
  pcd8544_fill_rect(lcd, 21, 16, (int16_t)(phase * 4U), 8, true);
  pcd8544_rect(lcd, 57, 13, 9, 9, true);
  pcd8544_fill_rect(lcd, 60, 16, 3, 3, phase >= 4U);
  pcd8544_line(lcd, 18, 36, 14, 40, true);
  pcd8544_line(lcd, 66, 36, 70, 40, true);
  draw_caption(lcd, startup);
}

void splash_play(pcd8544_t *lcd, bool startup) {
  const uint32_t mask = startup ? SPLASH_STARTUP_ANIMATION_MASK
                                : SPLASH_SHUTDOWN_ANIMATION_MASK;
  const uint8_t animation = choose_animation(mask);
  if (lcd == NULL) return;
  for (uint8_t frame = 0U; frame < SPLASH_FRAME_COUNT; ++frame) {
    pcd8544_clear(lcd);
    switch (animation) {
      case SPLASH_ANIMATION_EQUALIZER:
        draw_equalizer(lcd, frame, startup);
        break;
      case SPLASH_ANIMATION_ANTENNA:
        draw_antenna(lcd, frame, startup);
        break;
      case SPLASH_ANIMATION_POWER:
        draw_power(lcd, frame, startup);
        break;
      case SPLASH_ANIMATION_RADIO_DIAL:
      default:
        draw_radio_dial(lcd, frame, startup);
        break;
    }
    pcd8544_update(lcd);
    HAL_Delay(SPLASH_FRAME_TIME_MS);
  }
  HAL_Delay(SPLASH_FINAL_HOLD_MS);
}

bool splash_rle_decode(pcd8544_t *lcd, const uint8_t *data,
                       uint16_t data_length) {
  uint16_t source = 0U;
  uint16_t destination = 0U;
  if (lcd == NULL || data == NULL || (data_length & 1U) != 0U) return false;
  if (!pcd8544_wait_ready(lcd, 20U)) return false;
  while (source < data_length && destination < PCD8544_BUFFER_SIZE) {
    const uint8_t count = data[source++];
    const uint8_t value = data[source++];
    if (count == 0U || destination + count > PCD8544_BUFFER_SIZE) return false;
    memset(&lcd->buffer[destination], value, count);
    destination = (uint16_t)(destination + count);
  }
  return source == data_length && destination == PCD8544_BUFFER_SIZE;
}

#ifndef SPLASH_CONFIG_H
#define SPLASH_CONFIG_H

/*
 * Select animations by setting bits in the two masks.  Keeping fewer bits
 * enabled is also the quickest way to reduce splash code/data in a tight
 * build.  The built-in animations are original, monochrome pixel art.
 */
#define SPLASH_ANIMATION_RADIO_DIAL 0U
#define SPLASH_ANIMATION_EQUALIZER  1U
#define SPLASH_ANIMATION_ANTENNA    2U
#define SPLASH_ANIMATION_POWER      3U

#define SPLASH_ANIMATION_BIT(id) (1UL << (id))

#define SPLASH_STARTUP_ANIMATION_MASK                                      \
  (SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_RADIO_DIAL) |                    \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_EQUALIZER) |                     \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_ANTENNA) |                       \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_POWER))

#define SPLASH_SHUTDOWN_ANIMATION_MASK                                     \
  (SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_RADIO_DIAL) |                    \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_EQUALIZER) |                     \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_ANTENNA) |                       \
   SPLASH_ANIMATION_BIT(SPLASH_ANIMATION_POWER))

#define SPLASH_FRAME_TIME_MS 75U
#define SPLASH_FINAL_HOLD_MS 120U

#if SPLASH_STARTUP_ANIMATION_MASK == 0U || SPLASH_SHUTDOWN_ANIMATION_MASK == 0U
#error "At least one startup and one shutdown animation must be enabled"
#endif

#endif /* SPLASH_CONFIG_H */

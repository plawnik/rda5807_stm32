#include "app_config.h"
#include "rds_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t chars(char first, char second) {
  return (uint16_t)(((uint16_t)(uint8_t)first << 8) | (uint8_t)second);
}

static void test_frequency_rules(void) {
  radio_settings_t settings;
  radio_settings_defaults(&settings);
  assert(radio_band_min_khz(&settings) == 87000U);
  assert(radio_band_max_khz(&settings) == 108000U);
  assert(radio_spacing_khz(&settings) == 100U);
  assert(radio_frequency_clamp(&settings, 106149) == 106100U);
  assert(radio_frequency_clamp(&settings, 200000) == 108000U);
  assert(radio_frequency_step(&settings, 108000U, 100) == 87000U);
  settings.seek_stop_at_band = true;
  assert(radio_frequency_step(&settings, 108000U, 100) == 108000U);

  settings.band = RADIO_BAND_EAST;
  settings.east_band_starts_at_65mhz = false;
  settings.frequency_khz = 10U;
  settings.volume = 99U;
  settings.spacing = 99U;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 50000U);
  assert(settings.volume == 15U);
  assert(settings.spacing == RADIO_SPACING_25_KHZ);
}

static void test_program_service(void) {
  rds_decoder_t decoder;
  uint16_t blocks[4] = {0x1234U, 0U, 0U, 0U};
  static const char name[] = "RADIO123";
  rds_decoder_init(&decoder);
  for (uint8_t segment = 0U; segment < 4U; ++segment) {
    blocks[1] = segment;
    blocks[3] = chars(name[segment * 2U], name[segment * 2U + 1U]);
    rds_decoder_process(&decoder, blocks, 0U, 0U);
  }
  assert(decoder.ps_valid);
  assert(memcmp(decoder.program_service, name, 8U) == 0);
  assert(decoder.program_id == 0x1234U);
}

static void test_radio_text_and_ab_flag(void) {
  rds_decoder_t decoder;
  uint16_t blocks[4] = {0x2222U, 0U, 0U, 0U};
  rds_decoder_init(&decoder);

  blocks[1] = (uint16_t)(2U << 12);
  blocks[2] = chars('H', 'e'); blocks[3] = chars('l', 'l');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  blocks[1] = (uint16_t)((2U << 12) | 1U);
  blocks[2] = chars('o', ' '); blocks[3] = chars('R', 'D');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  blocks[1] = (uint16_t)((2U << 12) | 2U);
  blocks[2] = chars('S', '!'); blocks[3] = chars('\r', ' ');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(decoder.radio_text_valid);
  assert(strcmp(decoder.radio_text, "Hello RDS!") == 0);

  blocks[1] = (uint16_t)((2U << 12) | (1U << 4));
  blocks[2] = chars('N', 'o'); blocks[3] = chars('w', 'y');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(!decoder.radio_text_valid);
  assert(strncmp(decoder.radio_text, "Nowy", 4U) == 0);
}

static void test_rds_clock(void) {
  rds_decoder_t decoder;
  uint16_t blocks[4] = {0x3333U, 0U, 0U, 0U};
  const uint16_t mjd = 60000U;
  const uint8_t hour = 12U;
  const uint8_t minute = 30U;
  rds_decoder_init(&decoder);
  blocks[1] = (uint16_t)((4U << 12) | ((mjd >> 15) & 0x03U));
  blocks[2] = (uint16_t)(((mjd & 0x7FFFU) << 1) | (hour >> 4));
  blocks[3] = (uint16_t)(((hour & 0x0FU) << 12) | (minute << 6) | 2U);
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(decoder.clock_valid);
  assert(decoder.modified_julian_day == mjd);
  assert(decoder.local_hour == 13U);
  assert(decoder.local_minute == 30U);
}

int main(void) {
  test_frequency_rules();
  test_program_service();
  test_radio_text_and_ab_flag();
  test_rds_clock();
  puts("All host tests passed.");
  return 0;
}

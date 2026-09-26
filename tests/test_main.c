#include "app_config.h"
#include "input_policy.h"
#include "rds_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t chars(char first, char second) {
  return (uint16_t)(((uint16_t)(uint8_t)first << 8) | (uint8_t)second);
}

static void test_local_input_policy(void) {
  input_event_t event = {
      .rotation = 2,
      .click = true,
      .long_press = true,
      .left = true,
      .right = true,
      .ok = true,
      .ok_long_press = true,
  };
  input_event_t selected;

  selected = input_event_for_mode(event, RADIO_INPUT_ENCODER);
  assert(selected.rotation == 2);
  assert(!selected.left);
  assert(!selected.right);
  assert(selected.click);
  assert(selected.ok);
  assert(input_event_requests_standby(selected));

  selected = input_event_for_mode(event, RADIO_INPUT_BUTTONS);
  assert(selected.rotation == 0);
  assert(selected.left);
  assert(selected.right);
  assert(selected.click);
  assert(selected.ok);
  assert(input_event_requests_standby(selected));

  event.long_press = false;
  event.ok_long_press = false;
  assert(!input_event_requests_standby(event));
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

  settings.seek_stop_at_band = false;
  settings.spacing = RADIO_SPACING_50_KHZ;
  settings.frequency_khz = 106100U;
  assert(radio_frequency_step_channels(
             &settings, settings.frequency_khz, 1) == 106150U);
  assert(radio_frequency_step_channels(
             &settings, 106150U, -1) == 106100U);
  settings.frequency_khz = 106125U;
  settings.automatic_tuning = false;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 106150U);

  settings.band = RADIO_BAND_JAPAN;
  settings.frequency_khz = 108000U;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 91000U);

  settings.band = RADIO_BAND_EAST;
  settings.east_band_starts_at_65mhz = false;
  settings.frequency_khz = 60000U;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 60000U);
  settings.east_band_starts_at_65mhz = true;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 65000U);

  settings.band = RADIO_BAND_EAST;
  settings.east_band_starts_at_65mhz = false;
  settings.frequency_khz = 10U;
  settings.volume = 99U;
  settings.spacing = 99U;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == 50000U);
  assert(settings.volume == 15U);
  assert(settings.spacing == RADIO_SPACING_25_KHZ);

  radio_settings_defaults(&settings);
  assert(radio_settings_plan_frequency(&settings, 49900, 50U) == 50000U);
  assert(settings.band == RADIO_BAND_EAST);
  assert(!settings.east_band_starts_at_65mhz);
  assert(settings.spacing == RADIO_SPACING_50_KHZ);
  assert(radio_settings_plan_frequency(&settings, 75950, 50U) == 75950U);
  assert(settings.band == RADIO_BAND_EAST);
  assert(radio_settings_plan_frequency(&settings, 76000, 50U) == 76000U);
  assert(settings.band == RADIO_BAND_WORLD);
  assert(radio_settings_plan_frequency(&settings, 86950, 50U) == 86950U);
  assert(settings.band == RADIO_BAND_WORLD);
  assert(radio_settings_plan_frequency(&settings, 87000, 100U) == 87000U);
  assert(settings.band == RADIO_BAND_EU_US);
  assert(radio_settings_plan_frequency(&settings, 115000, 25U) == 115000U);
  assert(settings.spacing == RADIO_SPACING_50_KHZ);
  assert(radio_settings_plan_frequency(&settings, 200000, 100U) == 115000U);
  assert(radio_settings_plan_frequency(&settings, 105950, 50U) == 105950U);
  assert(settings.automatic_tuning);

  settings.extended_tuning = true;
  assert(radio_settings_plan_frequency(&settings, 138150, 50U) == 138150U);
  assert(settings.spacing == RADIO_SPACING_50_KHZ);
  assert(radio_settings_plan_frequency(&settings, 138200, 50U) == 138200U);
  assert(settings.spacing == RADIO_SPACING_100_KHZ);
  assert(radio_settings_plan_frequency(&settings, 189400, 50U) == 189400U);
  assert(settings.spacing == RADIO_SPACING_200_KHZ);
  assert(radio_settings_plan_frequency(&settings, INT32_MAX, 50U) ==
         RADIO_REGISTER_MAX_KHZ);
  assert(settings.spacing == RADIO_SPACING_200_KHZ);
  settings.extended_tuning = false;
  radio_settings_sanitize(&settings);
  assert(settings.frequency_khz == RADIO_SYNTH_MAX_KHZ);
}

static void test_program_service(void) {
  rds_decoder_t decoder;
  uint16_t blocks[4] = {0x1234U, 0U, 0U, 0U};
  static const char name[] = "RADIO123";
  rds_decoder_init(&decoder);
  for (uint8_t segment = 0U; segment < 4U; ++segment) {
    blocks[1] = segment;
    blocks[3] = chars(name[segment * 2U], name[segment * 2U + 1U]);
    for (uint8_t repeat = 0U; repeat < 3U; ++repeat) {
      rds_decoder_process(&decoder, blocks, 0U, 0U);
    }
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
  blocks[2] = chars('X', 'e'); blocks[3] = chars('l', 'l');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  blocks[2] = chars('H', 'e'); blocks[3] = chars('l', 'l');
  for (uint8_t repeat = 0U; repeat < 2U; ++repeat) {
    rds_decoder_process(&decoder, blocks, 0U, 0U);
  }
  assert(strncmp(decoder.radio_text, "Hell", 4U) == 0);
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  blocks[1] = (uint16_t)((2U << 12) | 1U);
  blocks[2] = chars('o', ' '); blocks[3] = chars('R', 'D');
  for (uint8_t repeat = 0U; repeat < 3U; ++repeat) {
    rds_decoder_process(&decoder, blocks, 0U, 0U);
  }
  blocks[1] = (uint16_t)((2U << 12) | 2U);
  blocks[2] = chars('S', '!'); blocks[3] = chars('\r', ' ');
  for (uint8_t repeat = 0U; repeat < 3U; ++repeat) {
    rds_decoder_process(&decoder, blocks, 0U, 0U);
  }
  assert(decoder.radio_text_valid);
  assert(strcmp(decoder.radio_text, "Hello RDS!") == 0);

  blocks[1] = (uint16_t)(2U << 12);
  blocks[2] = chars('J', 'u'); blocks[3] = chars('n', 'k');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(strcmp(decoder.radio_text, "Hello RDS!") == 0);

  blocks[1] = (uint16_t)((2U << 12) | (1U << 4));
  blocks[2] = chars('N', 'o'); blocks[3] = chars('w', 'y');
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(strcmp(decoder.radio_text, "Hello RDS!") == 0);
  for (uint8_t repeat = 0U; repeat < 3U; ++repeat) {
    rds_decoder_process(&decoder, blocks, 0U, 0U);
  }
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

static void test_program_id_hysteresis(void) {
  rds_decoder_t decoder;
  uint16_t blocks[4] = {0x1111U, 0U, 0U, chars('O', 'K')};
  rds_decoder_init(&decoder);
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(decoder.program_id == 0x1111U);
  assert(decoder.program_service[0] == 'O');

  blocks[0] = 0x2222U;
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(decoder.program_id == 0x1111U);
  assert(decoder.program_service[0] == 'O');

  blocks[0] = 0x1111U;
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  blocks[0] = 0x2222U;
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  rds_decoder_process(&decoder, blocks, 0U, 0U);
  assert(decoder.program_id == 0x2222U);
  assert(decoder.program_service[0] == ' ');
}

int main(void) {
  test_local_input_policy();
  test_frequency_rules();
  test_program_service();
  test_radio_text_and_ab_flag();
  test_rds_clock();
  test_program_id_hysteresis();
  puts("All host tests passed.");
  return 0;
}

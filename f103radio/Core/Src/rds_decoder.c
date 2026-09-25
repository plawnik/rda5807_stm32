#include "rds_decoder.h"

#include <string.h>

#define RDS_INITIAL_SEGMENT_CONFIDENCE 2U
#define RDS_MAX_SEGMENT_CONFIDENCE 3U

static char printable(uint8_t value) {
  /* The display font is ASCII. Unsupported RDS characters are made explicit. */
  return (value >= 0x20U && value <= 0x7EU) ? (char)value : ' ';
}

static void fill_spaces(char *text, uint8_t length) {
  memset(text, ' ', length);
  text[length] = '\0';
}

void rds_decoder_init(rds_decoder_t *decoder) {
  if (decoder == NULL) return;
  memset(decoder, 0, sizeof(*decoder));
  fill_spaces(decoder->program_service, 8U);
  fill_spaces(decoder->radio_text, 64U);
}

void rds_decoder_reset_station(rds_decoder_t *decoder) {
  if (decoder == NULL) return;
  rds_decoder_init(decoder);
}

static bool observe_u16(uint16_t *candidate, uint8_t *confidence,
                        uint16_t value) {
  if (*confidence == 0U) {
    *candidate = value;
    *confidence = 1U;
  } else if (*candidate == value) {
    if (*confidence < RDS_MAX_SEGMENT_CONFIDENCE) ++*confidence;
  } else if (*confidence > 1U) {
    --*confidence;
  } else {
    *candidate = value;
    *confidence = 1U;
  }
  return *confidence >= RDS_INITIAL_SEGMENT_CONFIDENCE;
}

static bool observe_u32(uint32_t *candidate, uint8_t *length,
                        uint8_t *confidence, uint32_t value,
                        uint8_t new_length) {
  if (*confidence == 0U) {
    *candidate = value;
    *length = new_length;
    *confidence = 1U;
  } else if (*candidate == value && *length == new_length) {
    if (*confidence < RDS_MAX_SEGMENT_CONFIDENCE) ++*confidence;
  } else if (*confidence > 1U) {
    --*confidence;
  } else {
    *candidate = value;
    *length = new_length;
    *confidence = 1U;
  }
  return *confidence >= RDS_INITIAL_SEGMENT_CONFIDENCE;
}

static void decode_group_0(rds_decoder_t *decoder, uint16_t block_b,
                           uint16_t block_d) {
  const uint8_t segment = (uint8_t)(block_b & 0x03U);
  const uint8_t offset = (uint8_t)(segment * 2U);

  decoder->traffic_announcement = (block_b & (1U << 4)) != 0U;
  decoder->music = (block_b & (1U << 3)) != 0U;
  if (observe_u16(&decoder->ps_candidate_segments[segment],
                  &decoder->ps_confidence[segment], block_d)) {
    decoder->program_service[offset] =
        printable((uint8_t)(decoder->ps_candidate_segments[segment] >> 8));
    decoder->program_service[offset + 1U] =
        printable((uint8_t)decoder->ps_candidate_segments[segment]);
    decoder->ps_segments |= (uint16_t)(1U << segment);
  }
  decoder->ps_valid = (decoder->ps_segments & 0x0FU) == 0x0FU;
}

static void clear_radio_text(rds_decoder_t *decoder, bool ab) {
  fill_spaces(decoder->radio_text, 64U);
  memset(decoder->rt_candidate_segments, 0,
         sizeof(decoder->rt_candidate_segments));
  memset(decoder->rt_confidence, 0, sizeof(decoder->rt_confidence));
  memset(decoder->rt_candidate_length, 0,
         sizeof(decoder->rt_candidate_length));
  decoder->rt_segments = 0U;
  decoder->radio_text_valid = false;
  decoder->text_ab = ab;
  decoder->text_ab_candidate = ab;
  decoder->text_ab_confidence = 0U;
  decoder->text_ab_seen = true;
}

static bool accept_text_ab(rds_decoder_t *decoder, bool ab) {
  if (!decoder->text_ab_seen) {
    decoder->text_ab = ab;
    decoder->text_ab_candidate = ab;
    decoder->text_ab_seen = true;
    return true;
  }
  if (decoder->text_ab == ab) {
    decoder->text_ab_candidate = ab;
    decoder->text_ab_confidence = 0U;
    return true;
  }
  if (decoder->text_ab_candidate != ab) {
    decoder->text_ab_candidate = ab;
    decoder->text_ab_confidence = 1U;
    return false;
  }
  if (decoder->text_ab_confidence < RDS_INITIAL_SEGMENT_CONFIDENCE) {
    ++decoder->text_ab_confidence;
  }
  if (decoder->text_ab_confidence < RDS_INITIAL_SEGMENT_CONFIDENCE) {
    return false;
  }
  clear_radio_text(decoder, ab);
  return true;
}

static void commit_rt_segment(rds_decoder_t *decoder, uint8_t segment,
                              uint8_t offset) {
  const uint8_t length = decoder->rt_candidate_length[segment];
  const uint32_t packed = decoder->rt_candidate_segments[segment];
  for (uint8_t index = 0U; index < length; ++index) {
    const uint8_t shift = (uint8_t)((length - 1U - index) * 8U);
    const uint8_t value = (uint8_t)(packed >> shift);
    if (value == 0x0DU) {
      decoder->radio_text[offset + index] = '\0';
      decoder->radio_text_valid = true;
      break;
    }
    decoder->radio_text[offset + index] = printable(value);
  }
}

static void decode_group_2(rds_decoder_t *decoder, uint16_t block_b,
                           uint16_t block_c, uint16_t block_d, bool version_b) {
  const bool ab = (block_b & (1U << 4)) != 0U;
  const uint8_t segment = (uint8_t)(block_b & 0x0FU);
  uint8_t offset;
  uint8_t length;
  uint32_t packed;

  /* A/B is a single vulnerable bit. Require two matching groups before a
   * text switch so one corrected/uncorrected block cannot blank the display. */
  if (!accept_text_ab(decoder, ab)) return;

  if (version_b) {
    offset = (uint8_t)(segment * 2U);
    length = 2U;
    packed = block_d;
  } else {
    offset = (uint8_t)(segment * 4U);
    length = 4U;
    packed = ((uint32_t)block_c << 16) | block_d;
  }
  if (observe_u32(&decoder->rt_candidate_segments[segment],
                  &decoder->rt_candidate_length[segment],
                  &decoder->rt_confidence[segment], packed, length)) {
    commit_rt_segment(decoder, segment, offset);
    decoder->rt_segments |= (uint16_t)(1U << segment);
  }
  if (decoder->rt_segments == 0xFFFFU) decoder->radio_text_valid = true;
}

static void decode_group_4a(rds_decoder_t *decoder, uint16_t block_b,
                            uint16_t block_c, uint16_t block_d) {
  int16_t minutes;
  const uint8_t utc_hour = (uint8_t)(((block_c & 1U) << 4) |
                                     ((block_d >> 12) & 0x0FU));
  const uint8_t utc_minute = (uint8_t)((block_d >> 6) & 0x3FU);
  const int8_t sign = (block_d & (1U << 5)) != 0U ? -1 : 1;
  const int8_t offset = (int8_t)(sign * (int8_t)(block_d & 0x1FU));

  if (utc_hour > 23U || utc_minute > 59U) return;
  decoder->modified_julian_day = (uint16_t)(((block_b & 0x03U) << 15) |
                                            (block_c >> 1));
  decoder->local_offset_half_hours = offset;
  minutes = (int16_t)((int16_t)utc_hour * 60 + utc_minute + offset * 30);
  while (minutes < 0) minutes += 24 * 60;
  while (minutes >= 24 * 60) minutes -= 24 * 60;
  decoder->local_hour = (uint8_t)(minutes / 60);
  decoder->local_minute = (uint8_t)(minutes % 60);
  decoder->clock_valid = true;
}

void rds_decoder_process(rds_decoder_t *decoder, const uint16_t blocks[4],
                         uint8_t bler_a, uint8_t bler_b) {
  uint16_t block_b;
  uint8_t group;
  bool version_b;

  if (decoder == NULL || blocks == NULL || bler_a >= 3U || bler_b >= 3U) {
    return;
  }

  if (decoder->program_id != 0U && decoder->program_id != blocks[0]) {
    if (decoder->program_id_candidate != blocks[0]) {
      decoder->program_id_candidate = blocks[0];
      decoder->program_id_confidence = 1U;
      return;
    }
    if (decoder->program_id_confidence < RDS_INITIAL_SEGMENT_CONFIDENCE) {
      ++decoder->program_id_confidence;
    }
    if (decoder->program_id_confidence < RDS_INITIAL_SEGMENT_CONFIDENCE) {
      return;
    }
    rds_decoder_reset_station(decoder);
  } else {
    decoder->program_id_candidate = blocks[0];
    decoder->program_id_confidence = 0U;
  }
  decoder->program_id = blocks[0];
  block_b = blocks[1];
  group = (uint8_t)((block_b >> 12) & 0x0FU);
  version_b = (block_b & (1U << 11)) != 0U;
  decoder->traffic_program = (block_b & (1U << 10)) != 0U;
  decoder->program_type = (uint8_t)((block_b >> 5) & 0x1FU);

  switch (group) {
    case 0U:
      decode_group_0(decoder, block_b, blocks[3]);
      break;
    case 2U:
      decode_group_2(decoder, block_b, blocks[2], blocks[3], version_b);
      break;
    case 4U:
      if (!version_b) decode_group_4a(decoder, block_b, blocks[2], blocks[3]);
      break;
    default:
      break;
  }
}

const char *rds_program_type_name(uint8_t program_type) {
  static const char *const names[32] = {
      "Brak", "Wiadomosci", "Aktualnosci", "Informacje",
      "Sport", "Edukacja", "Sluchowisko", "Kultura",
      "Nauka", "Rozne", "Pop", "Rock", "Muzyka lekka",
      "Lekka klasyka", "Klasyka", "Inna muzyka", "Pogoda",
      "Finanse", "Dla dzieci", "Spoleczne", "Religia",
      "Telefon", "Podroze", "Wypoczynek", "Jazz", "Country",
      "Muzyka krajowa", "Oldies", "Folk", "Dokument",
      "Test alarmu", "Alarm"};
  return names[program_type & 0x1FU];
}

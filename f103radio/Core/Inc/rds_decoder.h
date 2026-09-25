#ifndef RDS_DECODER_H
#define RDS_DECODER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char program_service[9];
  char radio_text[65];
  char ps_candidates[8];
  char rt_candidates[64];
  uint8_t ps_repetitions[8];
  uint8_t rt_repetitions[64];
  uint16_t program_id;
  uint16_t modified_julian_day;
  uint16_t ps_segments;
  uint16_t rt_segments;
  uint8_t program_type;
  uint8_t local_hour;
  uint8_t local_minute;
  int8_t local_offset_half_hours;
  bool traffic_program;
  bool traffic_announcement;
  bool music;
  bool ps_valid;
  bool radio_text_valid;
  bool clock_valid;
  bool text_ab;
  bool text_ab_seen;
} rds_decoder_t;

void rds_decoder_init(rds_decoder_t *decoder);
void rds_decoder_reset_station(rds_decoder_t *decoder);
void rds_decoder_process(rds_decoder_t *decoder, const uint16_t blocks[4],
                         uint8_t bler_a, uint8_t bler_b);
const char *rds_program_type_name(uint8_t program_type);

#endif /* RDS_DECODER_H */

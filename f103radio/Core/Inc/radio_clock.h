#ifndef RADIO_CLOCK_H
#define RADIO_CLOCK_H

#include "rds_decoder.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} radio_clock_time_t;

/* The STM32F1 RTC uses the internal LSI clock and keeps counting in STOP.
 * RDS CT supplies local date/time and periodically corrects LSI drift. */
bool radio_clock_init(void);
void radio_clock_sync_from_rds(const rds_decoder_t *rds);
bool radio_clock_read(radio_clock_time_t *time);

#endif /* RADIO_CLOCK_H */

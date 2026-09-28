#include "radio_clock.h"

#include "stm32f1xx_hal.h"

#include <limits.h>

#define RADIO_CLOCK_EPOCH_MJD 51544UL /* 2000-01-01 */
#define RADIO_CLOCK_BACKUP_MAGIC 0x5243U /* "RC" */
#define RADIO_CLOCK_VALID_MAGIC  0x4354U /* "CT" */
#define RADIO_CLOCK_WAIT_LIMIT 1000000UL
#define RADIO_CLOCK_RESYNC_TOLERANCE_SECONDS 90UL

static bool clock_ready;
static uint32_t last_ct_key = UINT32_MAX;

static bool wait_for_rtoff(void) {
  uint32_t timeout = RADIO_CLOCK_WAIT_LIMIT;
  while ((RTC->CRL & RTC_CRL_RTOFF) == 0U && timeout-- != 0U) {}
  return (RTC->CRL & RTC_CRL_RTOFF) != 0U;
}

static bool synchronize_registers(void) {
  uint32_t timeout = RADIO_CLOCK_WAIT_LIMIT;
  RTC->CRL &= ~RTC_CRL_RSF;
  while ((RTC->CRL & RTC_CRL_RSF) == 0U && timeout-- != 0U) {}
  return (RTC->CRL & RTC_CRL_RSF) != 0U;
}

static uint32_t read_counter(void) {
  uint32_t high_before;
  uint32_t high_after;
  uint32_t low;
  do {
    high_before = RTC->CNTH & 0xFFFFU;
    low = RTC->CNTL & 0xFFFFU;
    high_after = RTC->CNTH & 0xFFFFU;
  } while (high_before != high_after);
  return (high_before << 16) | low;
}

static bool write_counter(uint32_t value) {
  if (!wait_for_rtoff()) return false;
  RTC->CRL |= RTC_CRL_CNF;
  RTC->CNTH = value >> 16;
  RTC->CNTL = value & 0xFFFFU;
  RTC->CRL &= ~RTC_CRL_CNF;
  return wait_for_rtoff();
}

bool radio_clock_init(void) {
  uint32_t timeout = RADIO_CLOCK_WAIT_LIMIT;

  RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
  PWR->CR |= PWR_CR_DBP;
  RCC->CSR |= RCC_CSR_LSION;
  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U && timeout-- != 0U) {}
  if ((RCC->CSR & RCC_CSR_LSIRDY) == 0U) return false;

  if ((BKP->DR1 & 0xFFFFU) != RADIO_CLOCK_BACKUP_MAGIC ||
      (RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_BDCR_RTCSEL_LSI) {
    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;
    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) |
                RCC_BDCR_RTCSEL_LSI | RCC_BDCR_RTCEN;
    if (!synchronize_registers() || !wait_for_rtoff()) return false;
    RTC->CRL |= RTC_CRL_CNF;
    RTC->PRLH = ((LSI_VALUE - 1U) >> 16) & 0x0FU;
    RTC->PRLL = (LSI_VALUE - 1U) & 0xFFFFU;
    RTC->CNTH = 0U;
    RTC->CNTL = 0U;
    RTC->CRL &= ~RTC_CRL_CNF;
    if (!wait_for_rtoff()) return false;
    BKP->DR1 = RADIO_CLOCK_BACKUP_MAGIC;
    BKP->DR2 = 0U;
  } else {
    RCC->BDCR |= RCC_BDCR_RTCEN;
    if (!synchronize_registers()) return false;
  }
  clock_ready = true;
  return true;
}

void radio_clock_sync_from_rds(const rds_decoder_t *rds) {
  uint32_t day_seconds;
  uint32_t expected;
  uint32_t current;
  uint32_t difference;
  uint32_t ct_key;

  if (!clock_ready || rds == NULL || !rds->clock_valid ||
      rds->modified_julian_day < RADIO_CLOCK_EPOCH_MJD) {
    return;
  }
  day_seconds = (uint32_t)rds->local_hour * 3600UL +
                (uint32_t)rds->local_minute * 60UL;
  if ((uint32_t)rds->modified_julian_day - RADIO_CLOCK_EPOCH_MJD >
      (UINT32_MAX - day_seconds) / 86400UL) {
    return;
  }
  expected = ((uint32_t)rds->modified_julian_day - RADIO_CLOCK_EPOCH_MJD) *
                 86400UL +
             day_seconds;
  ct_key = ((uint32_t)rds->modified_julian_day << 11) |
           ((uint32_t)rds->local_hour << 6) | rds->local_minute;
  if (ct_key == last_ct_key) return;
  last_ct_key = ct_key;

  current = read_counter();
  difference = current > expected ? current - expected : expected - current;
  if ((BKP->DR2 & 0xFFFFU) != RADIO_CLOCK_VALID_MAGIC ||
      difference > RADIO_CLOCK_RESYNC_TOLERANCE_SECONDS) {
    if (write_counter(expected)) BKP->DR2 = RADIO_CLOCK_VALID_MAGIC;
  }
}

static void mjd_to_date(uint32_t mjd, radio_clock_time_t *time) {
  int32_t julian = (int32_t)mjd + 2400001 + 68569;
  int32_t century = (4 * julian) / 146097;
  int32_t year;
  int32_t month;
  int32_t day;

  julian -= (146097 * century + 3) / 4;
  year = (4000 * (julian + 1)) / 1461001;
  julian = julian - (1461 * year) / 4 + 31;
  month = (80 * julian) / 2447;
  day = julian - (2447 * month) / 80;
  julian = month / 11;
  month = month + 2 - 12 * julian;
  year = 100 * (century - 49) + year + julian;
  time->year = (uint16_t)year;
  time->month = (uint8_t)month;
  time->day = (uint8_t)day;
}

bool radio_clock_read(radio_clock_time_t *time) {
  uint32_t seconds;
  uint32_t day_seconds;
  if (!clock_ready || time == NULL ||
      (BKP->DR2 & 0xFFFFU) != RADIO_CLOCK_VALID_MAGIC) {
    return false;
  }
  seconds = read_counter();
  day_seconds = seconds % 86400UL;
  mjd_to_date(RADIO_CLOCK_EPOCH_MJD + seconds / 86400UL, time);
  time->hour = (uint8_t)(day_seconds / 3600UL);
  time->minute = (uint8_t)((day_seconds % 3600UL) / 60UL);
  time->second = (uint8_t)(day_seconds % 60UL);
  return true;
}

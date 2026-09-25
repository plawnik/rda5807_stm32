#include "settings_store.h"

#include "board_config.h"
#include "stm32f1xx_hal.h"

#include <stddef.h>
#include <string.h>

#define SETTINGS_MAGIC 0x52414449UL /* "RADI" */
#define SETTINGS_FORMAT_VERSION 3U
#define SETTINGS_LEGACY_VERSION 2U

/* Exact payload layout written by firmware format 2.  New fields are appended
 * to radio_settings_t, therefore the prefix can be migrated without changing
 * any of the old values. */
typedef struct {
  uint32_t frequency_khz;
  uint8_t volume;
  uint8_t band;
  uint8_t spacing;
  uint8_t seek_threshold;
  uint8_t old_seek_threshold;
  uint8_t softblend_threshold;
  uint8_t seek_mode;
  uint8_t lna_port;
  uint8_t lna_current;
  uint8_t lcd_contrast;
  uint8_t lcd_bias;
  bool muted;
  bool force_mono;
  bool bass_boost;
  bool rds_enabled;
  bool rbds_enabled;
  bool deemphasis_50us;
  bool softmute_enabled;
  bool softblend_enabled;
  bool afc_enabled;
  bool new_method_enabled;
  bool seek_stop_at_band;
  bool east_band_starts_at_65mhz;
  bool automatic_tuning;
  bool extended_tuning;
  bool lcd_inverted;
  bool lcd_backlight;
} radio_settings_v2_t;

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_size;
  uint32_t sequence;
  radio_settings_t settings;
  uint32_t crc32;
} settings_record_t;

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_size;
  uint32_t sequence;
  radio_settings_v2_t settings;
  uint32_t crc32;
} settings_record_v2_t;

_Static_assert((sizeof(settings_record_t) % 2U) == 0U,
               "Flash record must contain complete halfwords");
_Static_assert(sizeof(settings_record_t) <= SETTINGS_FLASH_PAGE_SIZE,
               "Settings record must fit in one flash page");
_Static_assert(sizeof(radio_settings_v2_t) ==
                   offsetof(radio_settings_t, rssi_average_ms),
               "Version 2 settings must match the current prefix");

uint32_t settings_store_crc32(const void *data, uint32_t length) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t i = 0U; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1U) != 0U ? 0xEDB88320UL : 0U);
    }
  }
  return ~crc;
}

static bool record_valid_v3(const settings_record_t *record) {
  if (record->magic != SETTINGS_MAGIC ||
      record->version != SETTINGS_FORMAT_VERSION ||
      record->payload_size != sizeof(radio_settings_t)) {
    return false;
  }
  return record->crc32 == settings_store_crc32(
      record, (uint32_t)offsetof(settings_record_t, crc32));
}

static bool record_valid_v2(const settings_record_v2_t *record) {
  if (record->magic != SETTINGS_MAGIC ||
      record->version != SETTINGS_LEGACY_VERSION ||
      record->payload_size != sizeof(radio_settings_v2_t)) {
    return false;
  }
  return record->crc32 == settings_store_crc32(
      record, (uint32_t)offsetof(settings_record_v2_t, crc32));
}

static bool page_valid(uint32_t address, uint32_t *sequence,
                       uint16_t *version) {
  const settings_record_t *current =
      (const settings_record_t *)(uintptr_t)address;
  const settings_record_v2_t *legacy =
      (const settings_record_v2_t *)(uintptr_t)address;
  if (record_valid_v3(current)) {
    *sequence = current->sequence;
    *version = SETTINGS_FORMAT_VERSION;
    return true;
  }
  if (record_valid_v2(legacy)) {
    *sequence = legacy->sequence;
    *version = SETTINGS_LEGACY_VERSION;
    return true;
  }
  return false;
}

static bool sequence_newer(uint32_t first, uint32_t second) {
  return (int32_t)(first - second) > 0;
}

void settings_store_init(settings_store_t *store) {
  if (store == NULL) return;
  memset(store, 0, sizeof(*store));
}

bool settings_store_load(settings_store_t *store, radio_settings_t *settings) {
  uint32_t a_sequence = 0U;
  uint32_t b_sequence = 0U;
  uint16_t a_version = 0U;
  uint16_t b_version = 0U;
  uint32_t selected_address = 0U;
  uint16_t selected_version = 0U;
  bool a_valid;
  bool b_valid;

  if (store == NULL || settings == NULL) return false;
  a_valid = page_valid(SETTINGS_FLASH_PAGE_A, &a_sequence, &a_version);
  b_valid = page_valid(SETTINGS_FLASH_PAGE_B, &b_sequence, &b_version);
  if (a_valid && b_valid) {
    if (sequence_newer(a_sequence, b_sequence)) {
      selected_address = SETTINGS_FLASH_PAGE_A;
      selected_version = a_version;
    } else {
      selected_address = SETTINGS_FLASH_PAGE_B;
      selected_version = b_version;
    }
  } else if (a_valid) {
    selected_address = SETTINGS_FLASH_PAGE_A;
    selected_version = a_version;
  } else if (b_valid) {
    selected_address = SETTINGS_FLASH_PAGE_B;
    selected_version = b_version;
  }

  if (selected_address == 0U) {
    radio_settings_defaults(settings);
    store->loaded_from_flash = false;
    return false;
  }

  if (selected_version == SETTINGS_FORMAT_VERSION) {
    const settings_record_t *record =
        (const settings_record_t *)(uintptr_t)selected_address;
    memcpy(settings, &record->settings, sizeof(*settings));
    store->sequence = record->sequence;
  } else {
    const settings_record_v2_t *record =
        (const settings_record_v2_t *)(uintptr_t)selected_address;
    radio_settings_defaults(settings);
    memcpy(settings, &record->settings, sizeof(record->settings));
    store->sequence = record->sequence;
  }
  radio_settings_sanitize(settings);
  store->active_page = selected_address;
  store->loaded_from_flash = true;
  return true;
}

bool settings_store_save(settings_store_t *store,
                         const radio_settings_t *settings) {
  settings_record_t record;
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;
  uint32_t target;
  HAL_StatusTypeDef result;
  const uint16_t *halfwords;

  if (store == NULL || settings == NULL) return false;
  target = store->active_page == SETTINGS_FLASH_PAGE_A
               ? SETTINGS_FLASH_PAGE_B
               : SETTINGS_FLASH_PAGE_A;

  memset(&record, 0, sizeof(record));
  record.magic = SETTINGS_MAGIC;
  record.version = SETTINGS_FORMAT_VERSION;
  record.payload_size = sizeof(radio_settings_t);
  record.sequence = store->sequence + 1U;
  memcpy(&record.settings, settings, sizeof(record.settings));
  record.crc32 = settings_store_crc32(
      &record, (uint32_t)offsetof(settings_record_t, crc32));

  HAL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = target;
  erase.NbPages = 1U;
  result = HAL_FLASHEx_Erase(&erase, &page_error);
  if (result == HAL_OK) {
    halfwords = (const uint16_t *)&record;
    for (uint32_t offset = 0U; offset < sizeof(record); offset += 2U) {
      result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, target + offset,
                                 halfwords[offset / 2U]);
      if (result != HAL_OK) break;
    }
  }
  HAL_FLASH_Lock();

  if (result != HAL_OK ||
      memcmp((const void *)(uintptr_t)target, &record, sizeof(record)) != 0) {
    return false;
  }
  store->active_page = target;
  store->sequence = record.sequence;
  store->loaded_from_flash = true;
  return true;
}

#include "settings_store.h"

#include "board_config.h"
#include "stm32f1xx_hal.h"

#include <stddef.h>
#include <string.h>

#define SETTINGS_MAGIC 0x52414449UL /* "RADI" */
#define SETTINGS_FORMAT_VERSION 1U

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_size;
  uint32_t sequence;
  radio_settings_t settings;
  uint32_t crc32;
} settings_record_t;

_Static_assert((sizeof(settings_record_t) % 2U) == 0U,
               "Flash record must contain complete halfwords");

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

static bool record_valid(const settings_record_t *record) {
  if (record->magic != SETTINGS_MAGIC ||
      record->version != SETTINGS_FORMAT_VERSION ||
      record->payload_size != sizeof(radio_settings_t)) {
    return false;
  }
  return record->crc32 == settings_store_crc32(
      record, (uint32_t)offsetof(settings_record_t, crc32));
}

static bool sequence_newer(uint32_t first, uint32_t second) {
  return (int32_t)(first - second) > 0;
}

void settings_store_init(settings_store_t *store) {
  if (store == NULL) return;
  memset(store, 0, sizeof(*store));
}

bool settings_store_load(settings_store_t *store, radio_settings_t *settings) {
  const settings_record_t *a =
      (const settings_record_t *)SETTINGS_FLASH_PAGE_A;
  const settings_record_t *b =
      (const settings_record_t *)SETTINGS_FLASH_PAGE_B;
  const bool a_valid = record_valid(a);
  const bool b_valid = record_valid(b);
  const settings_record_t *selected = NULL;

  if (store == NULL || settings == NULL) return false;
  if (a_valid && b_valid) selected = sequence_newer(a->sequence, b->sequence) ? a : b;
  else if (a_valid) selected = a;
  else if (b_valid) selected = b;

  if (selected == NULL) {
    radio_settings_defaults(settings);
    store->loaded_from_flash = false;
    return false;
  }

  memcpy(settings, &selected->settings, sizeof(*settings));
  radio_settings_sanitize(settings);
  store->active_page = (uint32_t)(uintptr_t)selected;
  store->sequence = selected->sequence;
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

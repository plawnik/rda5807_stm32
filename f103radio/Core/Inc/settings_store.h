#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include "app_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t active_page;
  uint32_t sequence;
  bool loaded_from_flash;
} settings_store_t;

void settings_store_init(settings_store_t *store);
bool settings_store_load(settings_store_t *store, radio_settings_t *settings);
bool settings_store_save(settings_store_t *store,
                         const radio_settings_t *settings);
uint32_t settings_store_crc32(const void *data, uint32_t length);

#endif /* SETTINGS_STORE_H */

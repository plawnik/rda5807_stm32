#ifndef PCD8544_H
#define PCD8544_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define PCD8544_WIDTH 84U
#define PCD8544_HEIGHT 48U
#define PCD8544_BUFFER_SIZE (PCD8544_WIDTH * PCD8544_HEIGHT / 8U)

typedef struct {
  uint8_t buffer[PCD8544_BUFFER_SIZE];
  SPI_HandleTypeDef *spi;
  uint8_t contrast;
  uint8_t bias;
  bool inverted;
  bool backlight;
  volatile bool transfer_active;
  volatile bool transfer_error;
} pcd8544_t;

void pcd8544_init(pcd8544_t *lcd, SPI_HandleTypeDef *spi, uint8_t contrast,
                  uint8_t bias, bool inverted, bool backlight);
void pcd8544_configure(pcd8544_t *lcd, uint8_t contrast, uint8_t bias,
                       bool inverted, bool backlight);
/* Starts one asynchronous 504-byte DMA transfer. Horizontal addressing sends
 * all six 84-byte display banks in one transaction. */
bool pcd8544_update(pcd8544_t *lcd);
bool pcd8544_wait_ready(pcd8544_t *lcd, uint32_t timeout_ms);
bool pcd8544_is_busy(const pcd8544_t *lcd);
void pcd8544_sleep(pcd8544_t *lcd);
void pcd8544_wake(pcd8544_t *lcd);
void pcd8544_clear(pcd8544_t *lcd);
void pcd8544_set_pixel(pcd8544_t *lcd, int16_t x, int16_t y, bool on);
void pcd8544_line(pcd8544_t *lcd, int16_t x0, int16_t y0, int16_t x1,
                  int16_t y1, bool on);
void pcd8544_rect(pcd8544_t *lcd, int16_t x, int16_t y, int16_t width,
                  int16_t height, bool on);
void pcd8544_fill_rect(pcd8544_t *lcd, int16_t x, int16_t y, int16_t width,
                       int16_t height, bool on);
void pcd8544_invert_rect(pcd8544_t *lcd, int16_t x, int16_t y,
                         int16_t width, int16_t height);
uint8_t pcd8544_char(pcd8544_t *lcd, int16_t x, int16_t y, char character,
                     uint8_t scale, bool on);
uint16_t pcd8544_text(pcd8544_t *lcd, int16_t x, int16_t y, const char *text,
                      uint8_t scale, bool on);

#endif /* PCD8544_H */

#include "pcd8544.h"

#include "board_config.h"
#include "stm32f1xx_hal.h"

#include <stdlib.h>
#include <string.h>

#define PCD8544_SPI_TIMEOUT_MS 20U
#define PCD8544_RESET_DELAY_MS 10U

static pcd8544_t *active_transfer_lcd;

/* Compact 5x7 ASCII font, one byte per vertical column. */
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},
    {0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},
    {0x08,0x04,0x08,0x10,0x08},{0x00,0x06,0x09,0x09,0x06}
};

static inline void pin_write(GPIO_TypeDef *port, uint16_t pin,
                             GPIO_PinState state) {
  port->BSRR = state == GPIO_PIN_SET ? pin : ((uint32_t)pin << 16U);
}

static bool send_bytes(pcd8544_t *lcd, bool data, const uint8_t *values,
                       uint16_t length) {
  HAL_StatusTypeDef status;
  if (lcd == NULL || lcd->spi == NULL || values == NULL || length == 0U) {
    return false;
  }
  if (!pcd8544_wait_ready(lcd, PCD8544_SPI_TIMEOUT_MS)) return false;
  pin_write(LCD_DC_GPIO_Port, LCD_DC_Pin,
            data ? GPIO_PIN_SET : GPIO_PIN_RESET);
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_RESET);
  lcd->transfer_error = false;
  status = HAL_SPI_Transmit(lcd->spi, values, length,
                            PCD8544_SPI_TIMEOUT_MS);
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
  if (status != HAL_OK) lcd->transfer_error = true;
  return status == HAL_OK;
}

static bool send_command(pcd8544_t *lcd, uint8_t value) {
  return send_bytes(lcd, false, &value, 1U);
}

static bool update_blocking(pcd8544_t *lcd) {
  static const uint8_t home_commands[] = {0x40U, 0x80U};
  return send_bytes(lcd, false, home_commands, sizeof(home_commands)) &&
         send_bytes(lcd, true, lcd->buffer, PCD8544_BUFFER_SIZE);
}

static void set_backlight(bool enabled) {
  GPIO_PinState state = enabled ? LCD_BACKLIGHT_ACTIVE_STATE
                                : (LCD_BACKLIGHT_ACTIVE_STATE == GPIO_PIN_SET
                                       ? GPIO_PIN_RESET
                                       : GPIO_PIN_SET);
  pin_write(LCD_BL_GPIO_Port, LCD_BL_Pin, state);
}

void pcd8544_init(pcd8544_t *lcd, SPI_HandleTypeDef *spi, uint8_t contrast,
                  uint8_t bias, bool inverted, bool backlight) {
  if (lcd == NULL || spi == NULL) return;
  memset(lcd, 0, sizeof(*lcd));
  lcd->spi = spi;
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
  pin_write(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  HAL_Delay(PCD8544_RESET_DELAY_MS);
  pin_write(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(PCD8544_RESET_DELAY_MS);
  pin_write(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(PCD8544_RESET_DELAY_MS);
  pcd8544_configure(lcd, contrast, bias, inverted, backlight);
  pcd8544_clear(lcd);
  /* The first frame is deliberately blocking. It proves the controller can
   * receive a complete 504-byte RAM image before DMA is used for normal UI
   * updates and avoids racing the first splash frame during power-up. */
  (void)update_blocking(lcd);
}

void pcd8544_configure(pcd8544_t *lcd, uint8_t contrast, uint8_t bias,
                       bool inverted, bool backlight) {
  if (lcd == NULL) return;
  lcd->contrast = contrast & 0x7FU;
  lcd->bias = bias & 0x07U;
  lcd->inverted = inverted;
  lcd->backlight = backlight;
  {
    const uint8_t commands[] = {
        0x21U, /* Extended instruction set. */
        (uint8_t)(0x80U | lcd->contrast),
        0x06U, /* Temperature coefficient 2. */
        (uint8_t)(0x10U | lcd->bias),
        0x20U, /* Basic set, horizontal addressing. */
        inverted ? 0x0DU : 0x0CU};
    (void)send_bytes(lcd, false, commands, sizeof(commands));
  }
  set_backlight(backlight);
}

bool pcd8544_update(pcd8544_t *lcd) {
  static const uint8_t home_commands[] = {0x40U, 0x80U};
  if (lcd == NULL || lcd->spi == NULL) return false;
  if (!send_bytes(lcd, false, home_commands, sizeof(home_commands))) {
    return false;
  }
  pin_write(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_RESET);
  lcd->transfer_error = false;
  lcd->transfer_active = true;
  active_transfer_lcd = lcd;
  if (HAL_SPI_Transmit_DMA(lcd->spi, lcd->buffer, PCD8544_BUFFER_SIZE) !=
      HAL_OK) {
    active_transfer_lcd = NULL;
    lcd->transfer_active = false;
    lcd->transfer_error = true;
    pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
    /* Keep the display useful even if DMA could not be started. */
    (void)HAL_SPI_Abort(lcd->spi);
    return update_blocking(lcd);
  }
  return true;
}

bool pcd8544_wait_ready(pcd8544_t *lcd, uint32_t timeout_ms) {
  uint32_t started_ms;
  if (lcd == NULL) return false;
  started_ms = HAL_GetTick();
  while (lcd->transfer_active) {
    if ((uint32_t)(HAL_GetTick() - started_ms) >= timeout_ms) {
      if (lcd->spi != NULL) (void)HAL_SPI_Abort(lcd->spi);
      active_transfer_lcd = NULL;
      lcd->transfer_active = false;
      lcd->transfer_error = true;
      pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
      return false;
    }
  }
  return true;
}

bool pcd8544_is_busy(const pcd8544_t *lcd) {
  return lcd != NULL && lcd->transfer_active;
}

void pcd8544_sleep(pcd8544_t *lcd) {
  if (lcd == NULL) return;
  (void)pcd8544_wait_ready(lcd, PCD8544_SPI_TIMEOUT_MS);
  set_backlight(false);
  send_command(lcd, 0x24U); /* Basic command set with power-down bit. */
}

void pcd8544_wake(pcd8544_t *lcd) {
  if (lcd == NULL) return;
  pcd8544_configure(lcd, lcd->contrast, lcd->bias, lcd->inverted,
                    lcd->backlight);
  pcd8544_clear(lcd);
  pcd8544_update(lcd);
}

void pcd8544_clear(pcd8544_t *lcd) {
  if (lcd != NULL) {
    /* DMA reads directly from this buffer; do not draw over an active frame. */
    (void)pcd8544_wait_ready(lcd, PCD8544_SPI_TIMEOUT_MS);
    memset(lcd->buffer, 0, sizeof(lcd->buffer));
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi) {
  pcd8544_t *lcd = active_transfer_lcd;
  if (lcd == NULL || lcd->spi != spi) return;
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
  lcd->transfer_active = false;
  active_transfer_lcd = NULL;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi) {
  pcd8544_t *lcd = active_transfer_lcd;
  if (lcd == NULL || lcd->spi != spi) return;
  pin_write(LCD_CE_GPIO_Port, LCD_CE_Pin, GPIO_PIN_SET);
  lcd->transfer_error = true;
  lcd->transfer_active = false;
  active_transfer_lcd = NULL;
}

void pcd8544_set_pixel(pcd8544_t *lcd, int16_t x, int16_t y, bool on) {
  uint16_t index;
  uint8_t mask;
  if (lcd == NULL || x < 0 || x >= (int16_t)PCD8544_WIDTH || y < 0 ||
      y >= (int16_t)PCD8544_HEIGHT) return;
  index = (uint16_t)x + ((uint16_t)y / 8U) * PCD8544_WIDTH;
  mask = (uint8_t)(1U << ((uint16_t)y & 7U));
  if (on) lcd->buffer[index] |= mask;
  else lcd->buffer[index] &= (uint8_t)~mask;
}

void pcd8544_line(pcd8544_t *lcd, int16_t x0, int16_t y0, int16_t x1,
                  int16_t y1, bool on) {
  const int16_t dx = (int16_t)abs(x1 - x0);
  const int16_t sx = x0 < x1 ? 1 : -1;
  const int16_t dy = (int16_t)-abs(y1 - y0);
  const int16_t sy = y0 < y1 ? 1 : -1;
  int16_t error = (int16_t)(dx + dy);
  while (true) {
    pcd8544_set_pixel(lcd, x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    const int16_t twice = (int16_t)(2 * error);
    if (twice >= dy) { error = (int16_t)(error + dy); x0 += sx; }
    if (twice <= dx) { error = (int16_t)(error + dx); y0 += sy; }
  }
}

void pcd8544_rect(pcd8544_t *lcd, int16_t x, int16_t y, int16_t width,
                  int16_t height, bool on) {
  if (width <= 0 || height <= 0) return;
  pcd8544_line(lcd, x, y, (int16_t)(x + width - 1), y, on);
  pcd8544_line(lcd, x, (int16_t)(y + height - 1),
               (int16_t)(x + width - 1), (int16_t)(y + height - 1), on);
  pcd8544_line(lcd, x, y, x, (int16_t)(y + height - 1), on);
  pcd8544_line(lcd, (int16_t)(x + width - 1), y,
               (int16_t)(x + width - 1), (int16_t)(y + height - 1), on);
}

void pcd8544_fill_rect(pcd8544_t *lcd, int16_t x, int16_t y, int16_t width,
                       int16_t height, bool on) {
  for (int16_t yy = y; yy < y + height; ++yy) {
    for (int16_t xx = x; xx < x + width; ++xx) pcd8544_set_pixel(lcd, xx, yy, on);
  }
}

void pcd8544_invert_rect(pcd8544_t *lcd, int16_t x, int16_t y,
                         int16_t width, int16_t height) {
  if (lcd == NULL) return;
  for (int16_t yy = y; yy < y + height; ++yy) {
    for (int16_t xx = x; xx < x + width; ++xx) {
      if (xx < 0 || xx >= (int16_t)PCD8544_WIDTH || yy < 0 ||
          yy >= (int16_t)PCD8544_HEIGHT) continue;
      const uint16_t index = (uint16_t)xx + ((uint16_t)yy / 8U) * PCD8544_WIDTH;
      lcd->buffer[index] ^= (uint8_t)(1U << ((uint16_t)yy & 7U));
    }
  }
}

uint8_t pcd8544_char(pcd8544_t *lcd, int16_t x, int16_t y, char character,
                     uint8_t scale, bool on) {
  uint8_t glyph;
  if ((uint8_t)character < 0x20U || (uint8_t)character > 0x7FU) character = '?';
  if (scale == 0U) scale = 1U;
  glyph = (uint8_t)character - 0x20U;
  for (uint8_t column = 0U; column < 5U; ++column) {
    for (uint8_t row = 0U; row < 7U; ++row) {
      if ((font5x7[glyph][column] & (1U << row)) == 0U) continue;
      pcd8544_fill_rect(lcd, (int16_t)(x + column * scale),
                        (int16_t)(y + row * scale), scale, scale, on);
    }
  }
  return (uint8_t)(6U * scale);
}

uint16_t pcd8544_text(pcd8544_t *lcd, int16_t x, int16_t y, const char *text,
                      uint8_t scale, bool on) {
  const int16_t start = x;
  if (text == NULL) return 0U;
  while (*text != '\0' && x < (int16_t)PCD8544_WIDTH) {
    x += pcd8544_char(lcd, x, y, *text++, scale, on);
  }
  return (uint16_t)(x - start);
}

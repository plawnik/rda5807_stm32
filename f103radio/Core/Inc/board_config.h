#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f1xx_hal.h"

/*
 * Central hardware map.  The PCD8544 is connected directly to the MCU. Pixel
 * drawing happens in a 504-byte RAM framebuffer and the complete frame is
 * transferred by SPI1 TX DMA; there is no I/O expander or I2C display bus.
 */
#define LCD_SCLK_GPIO_Port GPIOA
#define LCD_SCLK_Pin       GPIO_PIN_5  /* SPI1_SCK */
#define LCD_DIN_GPIO_Port  GPIOA
#define LCD_DIN_Pin        GPIO_PIN_7  /* SPI1_MOSI */
#define LCD_DC_GPIO_Port   GPIOB
#define LCD_DC_Pin         GPIO_PIN_8
#define LCD_CE_GPIO_Port   GPIOB
#define LCD_CE_Pin         GPIO_PIN_9
#define LCD_RST_GPIO_Port  GPIOB
#define LCD_RST_Pin        GPIO_PIN_12
#define LCD_BL_GPIO_Port   GPIOB
#define LCD_BL_Pin         GPIO_PIN_13

#define LCD_BACKLIGHT_ACTIVE_STATE GPIO_PIN_SET

/* Encoder A/B are connected to TIM2 (PA15/PB3).  The switch is active low. */
#define ENCODER_BUTTON_GPIO_Port GPIOB
#define ENCODER_BUTTON_Pin       GPIO_PIN_4
#define ENCODER_BUTTON_ACTIVE_STATE GPIO_PIN_RESET
#define ENCODER_COUNTS_PER_DETENT 4
#define ENCODER_DIRECTION         1

/* Optional three-button keypad.  Each button shorts the pin to GND. */
#define BUTTON_LEFT_GPIO_Port GPIOA
#define BUTTON_LEFT_Pin       GPIO_PIN_0
#define BUTTON_RIGHT_GPIO_Port GPIOA
#define BUTTON_RIGHT_Pin       GPIO_PIN_2
#define BUTTON_OK_GPIO_Port GPIOA
#define BUTTON_OK_Pin       GPIO_PIN_8
#define NAV_BUTTON_ACTIVE_STATE GPIO_PIN_RESET

/* Native USB Full Speed device (CDC virtual COM port). */
#define USB_DM_GPIO_Port GPIOA
#define USB_DM_Pin       GPIO_PIN_11
#define USB_DP_GPIO_Port GPIOA
#define USB_DP_Pin       GPIO_PIN_12

/* RDA5807M is connected to I2C2: PB10=SCL, PB11=SDA. */
#define RDA5807_I2C_TIMEOUT_MS 40U

/* Two 1 KiB pages reserved by the linker for persistent configuration. */
#define SETTINGS_FLASH_PAGE_A 0x0800F800UL
#define SETTINGS_FLASH_PAGE_B 0x0800FC00UL
#define SETTINGS_FLASH_PAGE_SIZE 1024UL

#endif /* BOARD_CONFIG_H */

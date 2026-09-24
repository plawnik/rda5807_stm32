#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f1xx_hal.h"

/*
 * Central hardware map.  The PCD8544 uses a direct, software-driven serial
 * interface; there is no I/O expander and no I2C display bus.
 */
#define LCD_SCLK_GPIO_Port GPIOB
#define LCD_SCLK_Pin       GPIO_PIN_6
#define LCD_DIN_GPIO_Port  GPIOB
#define LCD_DIN_Pin        GPIO_PIN_7
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

/* RDA5807M is connected to I2C2: PB10=SCL, PB11=SDA. */
#define RDA5807_I2C_TIMEOUT_MS 40U

/* Two 1 KiB pages reserved by the linker for persistent configuration. */
#define SETTINGS_FLASH_PAGE_A 0x0800F800UL
#define SETTINGS_FLASH_PAGE_B 0x0800FC00UL
#define SETTINGS_FLASH_PAGE_SIZE 1024UL

#endif /* BOARD_CONFIG_H */

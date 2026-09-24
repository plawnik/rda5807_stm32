#include "gpio.h"

#include "board_config.h"

void MX_GPIO_Init(void) {
  GPIO_InitTypeDef config = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_BOARD_GPIO_Port, LED_BOARD_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, LCD_SCLK_Pin | LCD_CE_Pin | LCD_RST_Pin,
                    GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, LCD_DIN_Pin | LCD_DC_Pin | LCD_BL_Pin,
                    GPIO_PIN_RESET);

  config.Pin = LED_BOARD_Pin;
  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_BOARD_GPIO_Port, &config);

  config.Pin = LCD_SCLK_Pin | LCD_DIN_Pin | LCD_DC_Pin | LCD_CE_Pin |
               LCD_RST_Pin | LCD_BL_Pin;
  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &config);

  config.Pin = ENCODER_BUTTON_Pin;
  config.Mode = GPIO_MODE_INPUT;
  config.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ENCODER_BUTTON_GPIO_Port, &config);
}

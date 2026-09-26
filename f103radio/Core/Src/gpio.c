#include "gpio.h"

#include "board_config.h"

void MX_GPIO_Init(void) {
  GPIO_InitTypeDef config = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_BOARD_GPIO_Port, LED_BOARD_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, LCD_CE_Pin | LCD_RST_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, LCD_DC_Pin | LCD_BL_Pin, GPIO_PIN_RESET);

  config.Pin = LED_BOARD_Pin;
  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_BOARD_GPIO_Port, &config);

  /* PA5 and PA7 are configured by HAL_SPI_MspInit(). */
  config.Pin = LCD_DC_Pin | LCD_CE_Pin | LCD_RST_Pin | LCD_BL_Pin;
  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &config);

  config.Pin = ENCODER_BUTTON_Pin;
  config.Mode = GPIO_MODE_IT_FALLING;
  config.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ENCODER_BUTTON_GPIO_Port, &config);

  config.Pin = BUTTON_LEFT_Pin | BUTTON_RIGHT_Pin;
  config.Mode = GPIO_MODE_INPUT;
  config.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &config);

  config.Pin = BUTTON_OK_Pin;
  config.Mode = GPIO_MODE_IT_FALLING;
  config.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BUTTON_OK_GPIO_Port, &config);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

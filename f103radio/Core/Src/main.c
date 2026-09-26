#include "main.h"

#include "board_config.h"
#include "dma.h"
#include "gpio.h"
#include "i2c.h"
#include "input.h"
#include "lcd_ui.h"
#include "pcd8544.h"
#include "radio_app.h"
#include "splash_animation.h"
#include "spi.h"
#include "terminal_ui.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"

static radio_app_t app;
static pcd8544_t lcd;
static lcd_ui_t lcd_ui;
static terminal_ui_t terminal_ui;
static input_t controls;

void SystemClock_Config(void);

static void enter_radio_standby(void) {
  splash_play(&lcd, false);
  radio_app_power_down(&app);
  pcd8544_sleep(&lcd);
  USB_DEVICE_DeInit();

  /* Ignore the press which requested shutdown. The next falling edge is the
   * wake-up request. */
  while (HAL_GPIO_ReadPin(ENCODER_BUTTON_GPIO_Port, ENCODER_BUTTON_Pin) ==
             ENCODER_BUTTON_ACTIVE_STATE ||
         HAL_GPIO_ReadPin(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin) ==
             NAV_BUTTON_ACTIVE_STATE) {
    HAL_Delay(5U);
  }
  HAL_Delay(30U);
  __HAL_GPIO_EXTI_CLEAR_IT(ENCODER_BUTTON_Pin);
  __HAL_GPIO_EXTI_CLEAR_IT(BUTTON_OK_Pin);
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

  SystemClock_Config();
  HAL_ResumeTick();
  HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  MX_USB_DEVICE_Init();
  pcd8544_wake(&lcd);
  splash_play(&lcd, true);
  radio_app_wake(&app, HAL_GetTick());
  input_resync(&controls, HAL_GetTick());
  lcd_ui_reset(&lcd_ui);
  terminal_ui_reset(&terminal_ui, HAL_GetTick());
}

int main(void) {
  uint32_t now_ms;
  input_event_t event;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_USB_DEVICE_Init();

  now_ms = HAL_GetTick();
  radio_app_init(&app, &hi2c2, now_ms);
  pcd8544_init(&lcd, &hspi1, app.settings.lcd_contrast, app.settings.lcd_bias,
               app.settings.lcd_inverted, app.settings.lcd_backlight);
  splash_play(&lcd, true);
  lcd_ui_init(&lcd_ui, &lcd, &app.settings);
  input_init(&controls, &htim2, now_ms);
  terminal_ui_init(&terminal_ui, &huart1, now_ms);

  while (1) {
    now_ms = HAL_GetTick();
    event = input_poll(&controls, now_ms);
    if ((app.settings.input_mode == RADIO_INPUT_ENCODER &&
         event.long_press) ||
        (app.settings.input_mode == RADIO_INPUT_BUTTONS &&
         event.ok_long_press)) {
      enter_radio_standby();
      continue;
    }
    lcd_ui_handle_input(&lcd_ui, &app, event, now_ms);
    terminal_ui_process(&terminal_ui, &app, now_ms);
    radio_app_process(&app, now_ms);
    lcd_ui_render(&lcd_ui, &app, now_ms);
    terminal_ui_render(&terminal_ui, &app, now_ms);
    HAL_Delay(1U);
  }
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};
  RCC_PeriphCLKInitTypeDef peripheral_clocks = {0};

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  oscillator.HSIState = RCC_HSI_ON;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) Error_Handler();

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV2;
  clocks.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  peripheral_clocks.PeriphClockSelection = RCC_PERIPHCLK_USB;
  peripheral_clocks.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clocks) != HAL_OK) Error_Handler();
}

void Error_Handler(void) {
  while (1) {
    HAL_GPIO_TogglePin(LED_BOARD_GPIO_Port, LED_BOARD_Pin);
    HAL_Delay(150U);
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
  (void)file;
  (void)line;
  Error_Handler();
}
#endif

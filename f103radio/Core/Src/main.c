#include "main.h"

#include "gpio.h"
#include "i2c.h"
#include "input.h"
#include "lcd_ui.h"
#include "pcd8544.h"
#include "radio_app.h"
#include "terminal_ui.h"
#include "tim.h"
#include "usart.h"

static radio_app_t app;
static pcd8544_t lcd;
static lcd_ui_t lcd_ui;
static terminal_ui_t terminal_ui;
static input_t controls;

void SystemClock_Config(void);

int main(void) {
  uint32_t now_ms;
  input_event_t event;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  now_ms = HAL_GetTick();
  radio_app_init(&app, &hi2c2, now_ms);
  pcd8544_init(&lcd, app.settings.lcd_contrast, app.settings.lcd_bias,
               app.settings.lcd_inverted, app.settings.lcd_backlight);
  lcd_ui_init(&lcd_ui, &lcd, &app.settings);
  input_init(&controls, &htim2, now_ms);
  terminal_ui_init(&terminal_ui, &huart1, now_ms);

  while (1) {
    now_ms = HAL_GetTick();
    event = input_poll(&controls, now_ms);
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

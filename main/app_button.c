#include "app_button.h"
#include "app_led.h"
#include "app_web.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "iot_button.h"
#include "sdkconfig.h"

#define TAG "app_button"
#define BUTTON_GPIO CONFIG_APP_BUTTON_GPIO

static void button_long_press_cb(void *arg, void *data) {
  ESP_LOGI(TAG, "Button Long Pressed - Reset device");
  app_led_set_state(APP_LED_IR_LEARN);
  esp_restart();
}

esp_err_t app_button_init(void) {
  button_config_t gpio_btn_cfg = {
      .type = BUTTON_TYPE_GPIO,
      .long_press_time = 2000,
      .short_press_time = 50,
      .gpio_button_config =
          {
              .gpio_num = BUTTON_GPIO,
              .active_level = CONFIG_APP_BUTTON_ACTIVE_LEVEL,
          },
  };

  button_handle_t btn_handle = iot_button_create(&gpio_btn_cfg);

  if (btn_handle) {
    iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START,
                           button_long_press_cb, NULL);
    ESP_LOGI(TAG, "Button initialized on GPIO %d", BUTTON_GPIO);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Button create failed");
    return ESP_FAIL;
  }
}

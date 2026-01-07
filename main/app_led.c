#include "app_led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "nvs.h"

#include "sdkconfig.h"
#include <math.h>
#include <stdlib.h>

#define TAG "app_led"
#define RGB_LED_GPIO CONFIG_APP_LED_GPIO

static led_strip_handle_t led_strip;
static app_led_state_t g_led_state = APP_LED_OFF;
static TaskHandle_t g_led_task_handle = NULL;

// Per-effect configuration structure
typedef struct {
  uint8_t colors[8][3]; // [led_index][rgb]
  uint8_t speed;        // 1-100
} app_led_effect_config_t;

static app_led_effect_config_t g_effect_configs[11]; // 10 effects + static
static app_led_effect_t g_current_effect = APP_LED_EFFECT_STATIC;
static uint8_t g_brightness = 100; // Global brightness

static app_led_state_config_t g_state_configs[10] = {
    [APP_LED_OFF] = {0, 0, 0},
    [APP_LED_STARTUP] = {0, 0, 0},     // Handled dynamically (Rainbow)
    [APP_LED_WIFI_PROV] = {0, 0, 20},  // Blue
    [APP_LED_WIFI_CONN] = {0, 20, 20}, // Cyan
    [APP_LED_OTA] = {10, 0, 10},       // Purple
    [APP_LED_IR_TX] = {20, 0, 0},      // Red
    [APP_LED_IR_LEARN] = {20, 20, 0},  // Yellow
    [APP_LED_IR_FAIL] = {20, 0, 0},    // Red
    [APP_LED_IR_SUCCESS] = {0, 20, 0}, // Green
    [APP_LED_IDLE] = {0, 20, 0},       // Default, overwritten by g_base_r/g/b
};

static void led_strip_set_pixel_scaled(int index, uint8_t r, uint8_t g,
                                       uint8_t b) {
  float scale = (float)g_brightness / 100.0f;
  led_strip_set_pixel(led_strip, index, (uint8_t)(r * scale),
                      (uint8_t)(g * scale), (uint8_t)(b * scale));
}

static void led_effect_rainbow(int step,
                               const app_led_effect_config_t *config) {
  uint8_t spd = config->speed > 0 ? config->speed : 50;
  int increment = (spd / 10) + 1; // logical speed
  for (int i = 0; i < 8; i++) {
    int hue = (step * increment + (i * 360 / 8)) % 360;
    // Simple HSV to RGB calculation
    float H = hue, S = 1.0, V = 1.0;
    float C = V * S;
    float X = C * (1 - fabs(fmod(H / 60.0, 2) - 1));
    float m = V - C;
    float Rs, Gs, Bs;

    if (H >= 0 && H < 60) {
      Rs = C;
      Gs = X;
      Bs = 0;
    } else if (H >= 60 && H < 120) {
      Rs = X;
      Gs = C;
      Bs = 0;
    } else if (H >= 120 && H < 180) {
      Rs = 0;
      Gs = C;
      Bs = X;
    } else if (H >= 180 && H < 240) {
      Rs = 0;
      Gs = X;
      Bs = C;
    } else if (H >= 240 && H < 300) {
      Rs = X;
      Gs = 0;
      Bs = C;
    } else {
      Rs = C;
      Gs = 0;
      Bs = X;
    }
    led_strip_set_pixel_scaled(i, (Rs + m) * 255, (Gs + m) * 255,
                               (Bs + m) * 255);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_running(int step,
                               const app_led_effect_config_t *config) {
  led_strip_clear(led_strip);
  int pos = step % 8;

  for (int i = 0; i < 3; i++) {
    int p = (pos - i + 8) % 8;
    float dim = 1.0 / (i + 1);
    // Use the color assigned to the LED at position 'p' instead of always
    // colors[0] Or closer to "running colorful", use the color from the config
    // for that index If user set colors[p], use that.
    uint8_t r = config->colors[p][0];
    uint8_t g = config->colors[p][1];
    uint8_t b = config->colors[p][2];

    led_strip_set_pixel_scaled(p, r * dim, g * dim, b * dim);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_breathing(int step,
                                 const app_led_effect_config_t *config) {
  float factor = 0.5 * (1 + sin(step * 0.1));
  uint8_t r = config->colors[0][0];
  uint8_t g = config->colors[0][1];
  uint8_t b = config->colors[0][2];

  for (int i = 0; i < 8; i++) {
    led_strip_set_pixel_scaled(i, r * factor, g * factor, b * factor);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_blink(int step, const app_led_effect_config_t *config) {
  if ((step / 10) % 2 == 0) {
    uint8_t r = config->colors[0][0];
    uint8_t g = config->colors[0][1];
    uint8_t b = config->colors[0][2];
    for (int i = 0; i < 8; i++) {
      led_strip_set_pixel_scaled(i, r, g, b);
    }
  } else {
    led_strip_clear(led_strip);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_knight_rider(int step,
                                    const app_led_effect_config_t *config) {
  led_strip_clear(led_strip);
  int n_leds = 8;
  int pos = step % ((n_leds - 1) * 2);
  if (pos >= n_leds)
    pos = (n_leds - 1) * 2 - pos;

  uint8_t r = config->colors[0][0] > 0 ? config->colors[0][0] : 255;
  uint8_t g = config->colors[0][1];
  uint8_t b = config->colors[0][2];

  for (int i = 0; i < n_leds; i++) {
    if (i == pos) {
      led_strip_set_pixel_scaled(i, r, g, b);
    } else if (abs(i - pos) == 1) {
      led_strip_set_pixel_scaled(i, r / 5, g / 5, b / 5);
    }
  }
  led_strip_refresh(led_strip);
}

static void led_effect_loading(int step,
                               const app_led_effect_config_t *config) {
  led_strip_clear(led_strip);
  int pos = step % 8;
  uint8_t r = config->colors[0][0];
  uint8_t g = config->colors[0][1] > 0 ? config->colors[0][1] : 100;
  uint8_t b = config->colors[0][2] > 0 ? config->colors[0][2] : 255;

  led_strip_set_pixel_scaled(pos, r, g, b);
  led_strip_set_pixel_scaled((pos + 1) % 8, r / 2, g / 2, b / 2);
  led_strip_refresh(led_strip);
}

static void led_effect_color_wipe(int step,
                                  const app_led_effect_config_t *config) {
  int pos = step % 9; // 0 to 8
  if (pos == 0)
    led_strip_clear(led_strip);
  uint8_t r = config->colors[0][0];
  uint8_t g = config->colors[0][1];
  uint8_t b = config->colors[0][2];

  for (int i = 0; i < pos; i++) {
    led_strip_set_pixel_scaled(i, r, g, b);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_theater_chase(int step,
                                     const app_led_effect_config_t *config) {
  led_strip_clear(led_strip);
  uint8_t r = config->colors[0][0];
  uint8_t g = config->colors[0][1];
  uint8_t b = config->colors[0][2];

  for (int i = 0; i < 8; i += 3) {
    led_strip_set_pixel_scaled((i + (step % 3)) % 8, r, g, b);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_fire(int step) {
  for (int i = 0; i < 8; i++) {
    int r = 200 + (rand() % 55);
    int g = 50 + (rand() % 100);
    led_strip_set_pixel_scaled(i, r, g, 0);
  }
  led_strip_refresh(led_strip);
}

static void led_effect_sparkle(int step,
                               const app_led_effect_config_t *config) {
  uint8_t r = config->colors[0][0];
  uint8_t g = config->colors[0][1];
  uint8_t b = config->colors[0][2];

  for (int i = 0; i < 8; i++) {
    led_strip_set_pixel_scaled(i, r, g, b);
  }
  int pixel = rand() % 8;
  led_strip_set_pixel_scaled(pixel, 255, 255, 255);
  led_strip_refresh(led_strip);
  led_strip_refresh(led_strip);
}

static void led_effect_random(int step) {
  if (step % 5 == 0) { // Change every 5 steps
    for (int i = 0; i < 8; i++) {
      led_strip_set_pixel_scaled(i, rand() % 256, rand() % 256, rand() % 256);
    }
    led_strip_refresh(led_strip);
  }
}

static void run_single_effect(app_led_effect_t effect, int step,
                              const app_led_effect_config_t *cfg) {
  uint8_t spd = cfg->speed;
  if (spd < 1)
    spd = 1;

  switch (effect) {
  case APP_LED_EFFECT_RAINBOW:
    led_effect_rainbow(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(110 - spd));
    break;
  case APP_LED_EFFECT_RUNNING:
    led_effect_running(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(110 - spd));
    break;
  case APP_LED_EFFECT_BREATHING:
    led_effect_breathing(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(50));
    break;
  case APP_LED_EFFECT_BLINK:
    led_effect_blink(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(100 - spd + 10));
    break;
  case APP_LED_EFFECT_KNIGHT_RIDER:
    led_effect_knight_rider(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(150 - spd));
    break;
  case APP_LED_EFFECT_LOADING:
    led_effect_loading(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(150 - spd));
    break;
  case APP_LED_EFFECT_COLOR_WIPE:
    led_effect_color_wipe(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(200 - spd * 2));
    break;
  case APP_LED_EFFECT_THEATER_CHASE:
    led_effect_theater_chase(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(150 - spd));
    break;
  case APP_LED_EFFECT_FIRE:
    led_effect_fire(step);
    vTaskDelay(pdMS_TO_TICKS(120 - spd));
    break;
  case APP_LED_EFFECT_SPARKLE:
    led_effect_sparkle(step, cfg);
    vTaskDelay(pdMS_TO_TICKS(100 - spd));
    break;
  case APP_LED_EFFECT_RANDOM:
    led_effect_random(step);
    vTaskDelay(pdMS_TO_TICKS(100 - spd));
    break;
  case APP_LED_EFFECT_STATIC:
  default:
    for (int i = 0; i < 8; i++) {
      led_strip_set_pixel_scaled(i, cfg->colors[i][0], cfg->colors[i][1],
                                 cfg->colors[i][2]);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    break;
  }
}

static void led_task_entry(void *arg) {
  int step = 0;
  // Cycle state
  int cycle_counter = 0;
  app_led_effect_t current_cycle_effect = APP_LED_EFFECT_RAINBOW;

  while (1) {
    if (!led_strip) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    switch (g_led_state) {
    case APP_LED_STARTUP: {
      app_led_effect_config_t cfg = {0};
      cfg.speed = 50;
      led_effect_rainbow(step, &cfg);
      vTaskDelay(pdMS_TO_TICKS(20));
      break;
    }
    case APP_LED_WIFI_PROV: {
      app_led_effect_config_t cfg; // Temp config for state
      cfg.colors[0][0] = g_state_configs[APP_LED_WIFI_PROV].r;
      cfg.colors[0][1] = g_state_configs[APP_LED_WIFI_PROV].g;
      cfg.colors[0][2] = g_state_configs[APP_LED_WIFI_PROV].b;
      led_effect_running(step, &cfg);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    }
    case APP_LED_WIFI_CONN: {
      app_led_effect_config_t cfg;
      cfg.colors[0][0] = g_state_configs[APP_LED_WIFI_CONN].r;
      cfg.colors[0][1] = g_state_configs[APP_LED_WIFI_CONN].g;
      cfg.colors[0][2] = g_state_configs[APP_LED_WIFI_CONN].b;
      led_effect_running(step, &cfg);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    }
    case APP_LED_OTA: {
      app_led_effect_config_t cfg;
      cfg.colors[0][0] = g_state_configs[APP_LED_OTA].r;
      cfg.colors[0][1] = g_state_configs[APP_LED_OTA].g;
      cfg.colors[0][2] = g_state_configs[APP_LED_OTA].b;
      led_effect_running(step, &cfg);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    }
    case APP_LED_IR_TX:
      // Flash
      if (step % 2 == 0) {
        for (int i = 0; i < 8; i++)
          led_strip_set_pixel_scaled(i, g_state_configs[APP_LED_IR_TX].r,
                                     g_state_configs[APP_LED_IR_TX].g,
                                     g_state_configs[APP_LED_IR_TX].b);
      } else {
        led_strip_clear(led_strip);
      }
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    case APP_LED_IR_LEARN:
      // Blink
      if (step % 10 < 5) {
        for (int i = 0; i < 8; i++)
          led_strip_set_pixel_scaled(i, g_state_configs[APP_LED_IR_LEARN].r,
                                     g_state_configs[APP_LED_IR_LEARN].g,
                                     g_state_configs[APP_LED_IR_LEARN].b);
      } else {
        led_strip_clear(led_strip);
      }
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    case APP_LED_IR_FAIL:
      // Blink Fast
      if (step % 4 < 2) {
        for (int i = 0; i < 8; i++)
          led_strip_set_pixel_scaled(i, g_state_configs[APP_LED_IR_FAIL].r,
                                     g_state_configs[APP_LED_IR_FAIL].g,
                                     g_state_configs[APP_LED_IR_FAIL].b);
      } else {
        led_strip_clear(led_strip);
      }
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    case APP_LED_IR_SUCCESS:
      // Solid
      for (int i = 0; i < 8; i++)
        led_strip_set_pixel_scaled(i, g_state_configs[APP_LED_IR_SUCCESS].r,
                                   g_state_configs[APP_LED_IR_SUCCESS].g,
                                   g_state_configs[APP_LED_IR_SUCCESS].b);
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    case APP_LED_IDLE: {
      const app_led_effect_config_t *cfg = &g_effect_configs[g_current_effect];

      if (g_current_effect == APP_LED_EFFECT_AUTO_CYCLE) {
        // Change effect every 400 iterations (approx 10-20 seconds)
        if (cycle_counter++ > 400) {
          cycle_counter = 0;
          // Pick random effect between RAINBOW and RANDOM (skip static)
          // Range: APP_LED_EFFECT_RAINBOW (1) to APP_LED_EFFECT_RANDOM (11)
          int min = APP_LED_EFFECT_RAINBOW;
          int max = APP_LED_EFFECT_RANDOM;
          current_cycle_effect =
              (app_led_effect_t)(min + (rand() % (max - min + 1)));
        }
        // Use config from the cycle effect if possible, or recycle current
        // config (speed) Ideally each effect uses its own saved config
        const app_led_effect_config_t *cycle_cfg =
            &g_effect_configs[current_cycle_effect];
        run_single_effect(current_cycle_effect, step, cycle_cfg);
      } else {
        run_single_effect(g_current_effect, step, cfg);
      }
    } break;
    case APP_LED_OFF:
    default:
      led_strip_clear(led_strip);
      led_strip_refresh(led_strip);
      vTaskDelay(pdMS_TO_TICKS(500));
      break;
    }
    step++;
  }
}

esp_err_t app_led_init(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = RGB_LED_GPIO,
      .max_leds = 8,
  };
  // Use SPI backend
  led_strip_spi_config_t spi_config = {
      .spi_bus = SPI2_HOST,
      .flags.with_dma = true,
  };
  ESP_ERROR_CHECK(
      led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
  led_strip_clear(led_strip);

  // Init default configs
  for (int i = 0; i < 11; i++) {
    g_effect_configs[i].speed = 50;
    for (int j = 0; j < 8; j++) {
      g_effect_configs[i].colors[j][0] = 0;
      g_effect_configs[i].colors[j][1] = 20; // Green default
      g_effect_configs[i].colors[j][2] = 0;
    }
  }

  xTaskCreate(led_task_entry, "led_task", 4096, NULL, 5, &g_led_task_handle);

  app_led_load_settings();

  return ESP_OK;
}

esp_err_t app_led_save_settings(void) {
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
    return err;

  err = nvs_set_blob(my_handle, "led_configs", g_effect_configs,
                     sizeof(g_effect_configs));
  if (err == ESP_OK)
    err = nvs_set_u8(my_handle, "led_effect", g_current_effect);

  // Also save brightness?
  if (err == ESP_OK)
    err = nvs_set_u8(my_handle, "led_bright", (uint8_t)g_brightness);

  if (err == ESP_OK)
    err = nvs_commit(my_handle);
  nvs_close(my_handle);
  return err;
}

esp_err_t app_led_load_settings(void) {
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
    return err;

  size_t required_size = sizeof(g_effect_configs);
  err =
      nvs_get_blob(my_handle, "led_configs", g_effect_configs, &required_size);

  uint8_t effect = 0;
  if (nvs_get_u8(my_handle, "led_effect", &effect) == ESP_OK) {
    g_current_effect = (app_led_effect_t)effect;
  }

  uint8_t bright = 100;
  if (nvs_get_u8(my_handle, "led_bright", &bright) == ESP_OK) {
    g_brightness = bright;
  }

  nvs_close(my_handle);
  return ESP_OK;
}

esp_err_t app_led_set_state(app_led_state_t state) {
  g_led_state = state;
  return ESP_OK;
}

esp_err_t app_led_set_base_color(uint8_t r, uint8_t g, uint8_t b) {
  // Backwards compat: set color for current effect
  for (int i = 0; i < 8; i++) {
    g_effect_configs[g_current_effect].colors[i][0] = r;
    g_effect_configs[g_current_effect].colors[i][1] = g;
    g_effect_configs[g_current_effect].colors[i][2] = b;
  }
  return ESP_OK;
}

esp_err_t app_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
  app_led_set_base_color(r, g, b);
  return ESP_OK;
}

esp_err_t app_led_set_effect(app_led_effect_t effect) {
  g_current_effect = effect;
  g_led_state = APP_LED_IDLE;
  return ESP_OK;
}

esp_err_t app_led_set_brightness(uint8_t brightness) {
  if (brightness > 100)
    brightness = 100;
  g_brightness = brightness;
  return ESP_OK;
}

esp_err_t app_led_set_speed(uint8_t speed) {
  if (speed > 100)
    speed = 100;
  if (speed < 1)
    speed = 1;
  g_effect_configs[g_current_effect].speed = speed;
  return ESP_OK;
}

esp_err_t app_led_set_config(app_led_effect_t effect, uint8_t index, uint8_t r,
                             uint8_t g, uint8_t b) {
  if (effect >= 11)
    return ESP_FAIL;
  if (index == 255) { // All
    for (int i = 0; i < 8; i++) {
      g_effect_configs[effect].colors[i][0] = r;
      g_effect_configs[effect].colors[i][1] = g;
      g_effect_configs[effect].colors[i][2] = b;
    }
  } else if (index < 8) {
    g_effect_configs[effect].colors[index][0] = r;
    g_effect_configs[effect].colors[index][1] = g;
    g_effect_configs[effect].colors[index][2] = b;
  }
  return ESP_OK;
}

esp_err_t app_led_get_effect_config(app_led_effect_t effect, uint8_t *speed,
                                    uint8_t colors[8][3]) {
  if (effect >= 11)
    return ESP_FAIL;
  if (speed)
    *speed = g_effect_configs[effect].speed;
  if (colors) {
    for (int i = 0; i < 8; i++) {
      colors[i][0] = g_effect_configs[effect].colors[i][0];
      colors[i][1] = g_effect_configs[effect].colors[i][1];
      colors[i][2] = g_effect_configs[effect].colors[i][2];
    }
  }
  return ESP_OK;
}

esp_err_t app_led_get_config(uint8_t *r, uint8_t *g, uint8_t *b,
                             app_led_effect_t *effect, uint8_t *brightness,
                             uint8_t *speed) {
  if (r)
    *r = g_effect_configs[g_current_effect].colors[0][0];
  if (g)
    *g = g_effect_configs[g_current_effect].colors[0][1];
  if (b)
    *b = g_effect_configs[g_current_effect].colors[0][2];
  if (effect)
    *effect = g_current_effect;
  if (brightness)
    *brightness = g_brightness;
  if (speed)
    *speed = g_effect_configs[g_current_effect].speed;
  return ESP_OK;
}

esp_err_t app_led_set_state_color(app_led_state_t state, uint8_t r, uint8_t g,
                                  uint8_t b) {
  if (state >= APP_LED_OFF && state < 10) { // 10 is implied MAX
    g_state_configs[state].r = r;
    g_state_configs[state].g = g;
    g_state_configs[state].b = b;
    return ESP_OK;
  }
  return ESP_FAIL;
}

esp_err_t app_led_get_state_color(app_led_state_t state, uint8_t *r, uint8_t *g,
                                  uint8_t *b) {
  if (state >= APP_LED_OFF && state < 10) {
    if (r)
      *r = g_state_configs[state].r;
    if (g)
      *g = g_state_configs[state].g;
    if (b)
      *b = g_state_configs[state].b;
    return ESP_OK;
  }
  return ESP_FAIL;
}

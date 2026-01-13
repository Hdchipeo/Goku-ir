#include "aht20_sensor.h"
#include "aht20.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

static const char *TAG = "goku_sensor";

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_PORT I2C_NUM_0

#if CONFIG_GOKU_SENSOR_AHT20

static i2c_bus_handle_t s_i2c_bus = NULL;
static aht20_dev_handle_t s_aht20 = NULL;

esp_err_t aht20_sensor_init(void) {
  if (s_aht20) {
    return ESP_OK;
  }

  // 1. Initialize I2C Bus using espressif/i2c_bus
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = CONFIG_APP_I2C_SDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = CONFIG_APP_I2C_SCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
      .clk_flags = 0,
  };

  s_i2c_bus = i2c_bus_create(I2C_MASTER_PORT, &conf);
  if (s_i2c_bus == NULL) {
    ESP_LOGE(TAG, "Failed to create I2C bus");
    return ESP_FAIL;
  }

  // 2. Initialize AHT20 Sensor using espressif/aht20
  aht20_i2c_config_t aht_conf = {
      .bus_inst = s_i2c_bus,
      .i2c_addr = AHT20_ADDRRES_0, // Try 0x38 first
  };

  esp_err_t err = aht20_new_sensor(&aht_conf, &s_aht20);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create AHT20 sensor: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "AHT20 Sensor Initialized (Official Driver)");
  return ESP_OK;
}

esp_err_t aht20_sensor_read(float *temp, float *hum) {
  if (!s_aht20) {
    return ESP_ERR_INVALID_STATE;
  }

  uint32_t t_raw, h_raw;
  esp_err_t err =
      aht20_read_temperature_humidity(s_aht20, &t_raw, temp, &h_raw, hum);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read AHT20: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGD(TAG, "Read: Temp %.1f C, Hum %.1f %%", *temp, *hum);
  return ESP_OK;
}

#else

esp_err_t aht20_sensor_init(void) { return ESP_OK; }
esp_err_t aht20_sensor_read(float *temp, float *hum) {
  return ESP_ERR_NOT_SUPPORTED;
}

#endif

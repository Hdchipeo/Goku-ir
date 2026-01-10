#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "ir_ac_registry.hpp"
#include "ir_engine.h"
#include "ir_protocol_nec.hpp"
#include "ir_universal.hpp"

static const char *TAG = "goku_ir_rmt";

static rmt_channel_handle_t g_tx_channel = NULL;
static rmt_encoder_handle_t g_copy_encoder = NULL;

extern "C" esp_err_t ir_engine_init(const ir_engine_config_t *config) {
  if (!config)
    return ESP_ERR_INVALID_ARG;

  ESP_LOGI(TAG, "Initializing IR Engine on GPIO %d", config->gpio_num);

  rmt_tx_channel_config_t tx_chan_config = {
      .gpio_num = (gpio_num_t)config->gpio_num,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = (uint32_t)config->resolution_hz,
      .mem_block_symbols = 64,
      .trans_queue_depth = 4,
  };

  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &g_tx_channel));

  rmt_carrier_config_t carrier_cfg = {
      .frequency_hz = 38000,
      .duty_cycle = 0.33,
  };
  ESP_ERROR_CHECK(rmt_apply_carrier(g_tx_channel, &carrier_cfg));
  ESP_ERROR_CHECK(rmt_enable(g_tx_channel));

  // Create Copy Encoder (Standard IDF)
  rmt_copy_encoder_config_t copy_encoder_config = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &g_copy_encoder));

  return ESP_OK;
}

extern "C" esp_err_t ir_engine_send_raw(const void *symbols, size_t count) {
  if (!g_tx_channel || !g_copy_encoder) {
    return ESP_ERR_INVALID_STATE;
  }

  rmt_transmit_config_t tx_config = {.loop_count = 0};
  ESP_ERROR_CHECK(rmt_transmit(g_tx_channel, g_copy_encoder, symbols,
                               count * sizeof(rmt_symbol_word_t), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(g_tx_channel, -1));

  return ESP_OK;
}

extern "C" esp_err_t ir_engine_send_nec(uint16_t address, uint16_t command) {
  if (!g_tx_channel || !g_copy_encoder) {
    return ESP_ERR_INVALID_STATE;
  }

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_nec_generate_symbols(address, command, &symbol_count);

  if (!symbols || symbol_count == 0) {
    if (symbols)
      free(symbols);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending NEC: Addr=0x%04X, Cmd=0x%04X, Symbols=%d", address,
           command, (int)symbol_count);
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

#include "ir_protocol_daikin.hpp"

extern "C" esp_err_t ir_engine_send_daikin(const ir_ac_state_t *state) {
  if (!g_tx_channel || !g_copy_encoder)
    return ESP_ERR_INVALID_STATE;

  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols = ir_daikin_generate_symbols(state, &symbol_count);

  if (!symbols)
    return ESP_FAIL;

  ESP_LOGI(TAG, "Sending Daikin AC: P=%d T=%d M=%d", state->power, state->temp,
           state->mode);

  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

#include "ir_protocol_mitsubishi.hpp"
#include "ir_protocol_samsung.hpp"

extern "C" esp_err_t ir_engine_send_samsung(const ir_ac_state_t *state) {
  if (!g_tx_channel || !g_copy_encoder)
    return ESP_ERR_INVALID_STATE;
  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_samsung_generate_symbols(state, &symbol_count);
  if (!symbols)
    return ESP_FAIL;
  ESP_LOGI(TAG, "Sending Samsung AC");
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

extern "C" esp_err_t ir_engine_send_mitsubishi(const ir_ac_state_t *state) {
  if (!g_tx_channel || !g_copy_encoder)
    return ESP_ERR_INVALID_STATE;
  size_t symbol_count = 0;
  rmt_symbol_word_t *symbols =
      ir_mitsubishi_generate_symbols(state, &symbol_count);
  if (!symbols)
    return ESP_FAIL;
  ESP_LOGI(TAG, "Sending Mitsubishi AC");
  esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
  free(symbols);
  return err;
}

extern "C" esp_err_t ir_engine_send_ac(ac_brand_t brand,
                                       const ir_ac_state_t *state) {
  if (!g_tx_channel || !g_copy_encoder || !state)
    return ESP_ERR_INVALID_STATE;

  // 1. Try Universal Registry first
  const ir_ac_definition_t *def = ir_ac_registry_get(brand);
  if (def) {
    ESP_LOGI(TAG, "Using Universal Engine for Brand %d (%s)", brand,
             def->protocol.name);

    // Translate State -> Bytes
    uint8_t payload[32] = {0}; // Reasonable max for AC
    size_t payload_len = 0;

    if (def->translator) {
      def->translator(state, payload, &payload_len);
    }

    if (payload_len == 0) {
      ESP_LOGE(TAG, "Translator failed to generate payload");
      return ESP_FAIL;
    }

    // Generate Symbols
    size_t symbol_count = 0;
    rmt_symbol_word_t *symbols = ir_universal_generate_symbols(
        &def->protocol, payload, payload_len, &symbol_count);

    if (!symbols)
      return ESP_ERR_NO_MEM;

    esp_err_t err = ir_engine_send_raw(symbols, symbol_count);
    free(symbols);
    return err;
  }

  // 2. Fallback to Legacy Handlers
  ESP_LOGW(TAG, "Brand %d not in Registry, trying legacy...", brand);
  switch (brand) {
  case AC_BRAND_DAIKIN:
    return ir_engine_send_daikin(state);
  case AC_BRAND_MITSUBISHI:
    return ir_engine_send_mitsubishi(state);
  case AC_BRAND_SAMSUNG:
    // Should be in registry now, but keep as backup if registry lookup fails?
    return ir_engine_send_samsung(state);
  default:
    return ESP_ERR_NOT_SUPPORTED;
  }
}

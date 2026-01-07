#include "app_ir.h"
#include "app_data.h"
#include "app_led.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h" // Added for restart timer
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
// #include "ir_encoder.h" // Removed external dependency
#include <esp_rom_sys.h> // For ets_printf
#include <inttypes.h>
#include <string.h>

#define TAG "app_ir"
#define IR_RX_GPIO CONFIG_APP_IR_RX_GPIO
#define IR_TX_GPIO CONFIG_APP_IR_TX_GPIO
#define RMT_RESOLUTION_HZ 1000000 // 1MHz, 1 tick = 1us
#define APP_IR_MIN_SYMBOLS 20     // Minimum symbols to be considered valid IR

static rmt_channel_handle_t s_rx_channel = NULL;
static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_ir_encoder = NULL;
static esp_timer_handle_t s_restart_timer = NULL;

#include "esp_heap_caps.h" // Added for heap_caps_malloc

// Data storage
#define MAX_IR_SYMBOLS 600 // Safe size for DMA (Max < 4095 bytes)

// Dynamic buffer for learning
static rmt_symbol_word_t *s_learning_symbols = NULL;
static uint32_t s_learning_num_symbols = 0;
static bool s_is_learning = false;

// Forward declaration
static void app_ir_restart_reception(void *arg);

// --- Memory Helper ---
static void *app_ir_malloc(size_t size) {
  // Try PSRAM first (if configured and available)
  // MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT handles external RAM
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ptr == NULL) {
    // Fallback to internal memory
    ptr = malloc(size);
  }
  return ptr;
}

// --- Palette-based Compression ---

#define IR_DATA_MAGIC 0xA5
#define PALETTE_TOLERANCE 100 // 100us tolerance for grouping

typedef struct {
  uint16_t duration;
} ir_palette_item_t;

// Find or add duration to palette
static int app_ir_get_palette_idx(uint16_t duration, ir_palette_item_t *palette,
                                  uint8_t *palette_size, uint8_t max_palette) {
  for (int i = 0; i < *palette_size; i++) {
    if (abs((int)palette[i].duration - (int)duration) <= PALETTE_TOLERANCE) {
      return i;
    }
  }
  if (*palette_size < max_palette) {
    palette[*palette_size].duration = duration;
    (*palette_size)++;
    return (*palette_size) - 1;
  }
  return -1; // Palette full
}

/**
 * @brief Encode IR symbols using Palette-based compression (4-bit nibbles)
 * Format: [Magic:1][Count:4][PalSize:1][Palette:P*2][Data: ceil(N/2)]
 */
static size_t app_ir_encode(const rmt_symbol_word_t *src, uint32_t count,
                            uint8_t *dst, size_t max_len) {
  if (count == 0)
    return 0;

  ir_palette_item_t palette[16];
  uint8_t palette_size = 0;
  uint8_t *indices = (uint8_t *)app_ir_malloc(count); // Use app_ir_malloc
  if (!indices)
    return 0;

  // 1. Build Palette and Indices
  const uint16_t *src_items = (const uint16_t *)src; // Treat as u16 stream
  bool success = true;

  for (uint32_t i = 0; i < count; i++) {
    uint16_t val = src_items[i];
    uint16_t duration = val & 0x7FFF; // Ignore level for quantization

    int idx = app_ir_get_palette_idx(duration, palette, &palette_size, 16);
    if (idx < 0) {
      // Fallback: If palette full, maybe too complex signal?
      // For AC, 16 distinct durations is usually enough (Header, 0, 1, Repeat,
      // End). If failed, we could fallback to raw or 8-bit, but 16 should cover
      // 99% ACs.
      ESP_LOGW(TAG, "IR Compression: Palette overflow (>16 unique durations)");
      success = false;
      break;
    }
    indices[i] = (uint8_t)idx;
  }

  if (!success) {
    free(indices);
    return 0;
  }

  // 2. Calculate Size
  size_t data_len = (count + 1) / 2; // Packed 4-bit
  size_t total_len = 1 + 4 + 1 + (palette_size * 2) + data_len;

  if (total_len > max_len) {
    free(indices);
    return 0; // Buffer too small
  }

  // 3. Serialize
  if (dst) {
    uint8_t *p = dst;
    *p++ = IR_DATA_MAGIC;
    memcpy(p, &count, 4);
    p += 4;
    *p++ = palette_size;
    for (int i = 0; i < palette_size; i++) {
      memcpy(p, &palette[i].duration, 2);
      p += 2;
    }

    memset(p, 0, data_len);
    for (uint32_t i = 0; i < count; i++) {
      if (i % 2 == 0) {
        p[i / 2] |= (indices[i] & 0x0F) << 4; // High nibble
      } else {
        p[i / 2] |= (indices[i] & 0x0F); // Low nibble
      }
    }
  }

  free(indices);
  return total_len;
}

static size_t app_ir_decode(const uint8_t *src, size_t src_len,
                            rmt_symbol_word_t *dst, size_t dst_max_bytes) {
  if (src_len < 6)
    return 0; // Header size

  const uint8_t *p = src;
  if (*p++ != IR_DATA_MAGIC)
    return 0; // Invalid Magic

  uint32_t count;
  memcpy(&count, p, 4);
  p += 4;
  uint8_t palette_size = *p++;

  if (src_len < (6 + palette_size * 2))
    return 0;

  // Read Palette
  uint16_t palette[16];
  for (int i = 0; i < palette_size; i++) {
    memcpy(&palette[i], p, 2);
    p += 2;
  }

  // Check output buffer
  size_t required_dst_bytes = count * sizeof(uint16_t);
  // Ensure we unpack into rmt_symbol_word_t correctly (which is what dst points
  // to) But wait, the caller usually expects rmt_symbol_word_t array. We need
  // to verify if dst_max_bytes matches.
  if (required_dst_bytes > dst_max_bytes)
    return 0;

  // Unpack
  // C3/S3: rmt_symbol_word_t is 32-bit (2 items). We treat dst as uint16_t*
  // stream.
  uint16_t *dst_u16 = (uint16_t *)dst;

  int data_idx = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint8_t idx;
    if (i % 2 == 0) {
      idx = (p[data_idx] >> 4) & 0x0F;
    } else {
      idx = (p[data_idx] & 0x0F);
      data_idx++;
    }

    if (idx >= palette_size)
      idx = 0; // Correction

    uint16_t duration = palette[idx];
    uint16_t level =
        (i % 2 == 0) ? 1 : 0; // Reconstruct implicit level: Mark, Space...

    // Wait! Raw capture might not start with Mark=1 always, but usually yes for
    // IR. However, the original capture had levels. Optimisation: We assumed
    // strict alternation. If capture was garbage check? For now, standard IR is
    // Modulated(1) then Space(0).

    dst_u16[i] = duration | (level << 15);
  }

  return count; // Return number of symbols decoded
}

// RX Callback
static bool app_ir_rx_done_callback(rmt_channel_handle_t rx_chan,
                                    const rmt_rx_done_event_data_t *edata,
                                    void *user_ctx) {
  if (s_is_learning) {
    if (edata->num_symbols < APP_IR_MIN_SYMBOLS) {
      esp_rom_printf("IR Noise: %d symbols. Relearning...\n",
                     (int)edata->num_symbols);
      app_led_set_state(APP_LED_IR_FAIL);            // Set fail LED
      esp_timer_start_once(s_restart_timer, 500000); // 500ms delay
    } else {
      // Valid data received
      s_learning_num_symbols = (edata->num_symbols > MAX_IR_SYMBOLS)
                                   ? MAX_IR_SYMBOLS
                                   : edata->num_symbols;

      // No need to memcpy if s_learning_symbols was passed to rmt_receive
      // The driver writes directly to it.

      esp_rom_printf("IR RX Valid! Symbols: %d\n", (int)s_learning_num_symbols);

      // Removed verbose logging

      // Auto-stop learning with success indication
      s_is_learning = false;
      app_led_set_state(APP_LED_IR_SUCCESS);          // Show success color
      esp_timer_start_once(s_restart_timer, 1000000); // 1s delay
    }
  }
  return false;
}

static void app_ir_restart_reception(void *arg) {
  if (!s_is_learning) {
    app_led_set_state(APP_LED_IDLE);
    return;
  }
  if (!s_learning_symbols)
    return;

  rmt_receive_config_t receive_config = {
      .signal_range_min_ns = 1250,
      .signal_range_max_ns = 30000000, // 30ms (Max hardware limit is ~32.7ms)
  };
  app_led_set_state(APP_LED_IR_LEARN);

  // Use MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t) for size
  ESP_ERROR_CHECK(rmt_receive(s_rx_channel, s_learning_symbols,
                              MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t),
                              &receive_config));
}

esp_err_t app_ir_init(void) {
  ESP_LOGI(TAG, "Initializing IR Hardware...");

  // Alloc Learning Buffer
  // RMT driver requires internal RAM for receive buffer even with DMA on some
  // targets/versions
  s_learning_symbols = (rmt_symbol_word_t *)heap_caps_malloc(
      MAX_IR_SYMBOLS * sizeof(rmt_symbol_word_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!s_learning_symbols) {
    ESP_LOGE(TAG, "Failed to alloc learning buffer");
    return ESP_ERR_NO_MEM;
  }

  // 1. Initialize RX Channel
  ESP_LOGI(TAG, "Initializing RX Channel on GPIO %d...", IR_RX_GPIO);
  rmt_rx_channel_config_t rx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RMT_RESOLUTION_HZ,
      .mem_block_symbols = 64, // 64 symbols per channel
      .gpio_num = IR_RX_GPIO,
      .flags.invert_in = 1, // Active Low
#ifndef CONFIG_IDF_TARGET_ESP32C3
      .flags.with_dma = 1, // Enable DMA for large buffers
#endif
  };
  ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &s_rx_channel));

  // CRITICAL: IR Receiver requires Pull-Up (Probe worked because it enabled it)
  gpio_set_pull_mode(IR_RX_GPIO, GPIO_PULLUP_ONLY);

  // Verify RX channel creation
  if (!s_rx_channel) {
    ESP_LOGE(TAG, "Failed to create RX channel");
    return ESP_FAIL;
  }

  // Register RX callbacks
  rmt_rx_event_callbacks_t cbs = {
      .on_recv_done = app_ir_rx_done_callback,
  };
  ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_channel, &cbs, NULL));
  ESP_ERROR_CHECK(rmt_enable(s_rx_channel));

  // 2. Initialize TX Channel
  ESP_LOGI(TAG, "Initializing TX Channel on GPIO %d...", IR_TX_GPIO);
  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .gpio_num = IR_TX_GPIO,
      .mem_block_symbols = 64, // 64 symbols
      .resolution_hz = RMT_RESOLUTION_HZ,
      .trans_queue_depth = 4,
#ifndef CONFIG_IDF_TARGET_ESP32C3
      .flags.with_dma = 1, // Enable DMA for large buffers (S3 only)
#endif
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &s_tx_channel));

  // Enable Carrier Modulation (38kHz) for IR
  rmt_carrier_config_t carrier_cfg = {
      .duty_cycle = 0.33,    // 33% Duty Cycle
      .frequency_hz = 38000, // 38kHz
      .flags.polarity_active_low =
          false, // Output is Active High (Mark = Carrier ON)
  };
  ESP_ERROR_CHECK(rmt_apply_carrier(s_tx_channel, &carrier_cfg));

  ESP_ERROR_CHECK(rmt_enable(s_tx_channel));

  // 3. Initialize Encoder (Use standard Copy Encoder to avoid external
  // dependencies)
  rmt_copy_encoder_config_t encoder_config = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &s_ir_encoder));

  // 4. Initialize Restart Timer
  esp_timer_create_args_t timer_args = {
      .callback = app_ir_restart_reception,
      .name = "ir_restart",
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_restart_timer));

  ESP_LOGI(TAG, "IR Initialized (Native RMT + Copy Encoder)");
  return ESP_OK;
}

esp_err_t app_ir_start_learn(void) {
  if (!s_rx_channel)
    return ESP_ERR_INVALID_STATE;

  ESP_LOGI(TAG, "Starting IR Learn...");
  s_is_learning = true;
  s_learning_num_symbols = 0; // Reset
  app_led_set_state(APP_LED_IR_LEARN);

  app_ir_restart_reception(NULL);
  return ESP_OK;
}

esp_err_t app_ir_stop_learn(void) {
  s_is_learning = false;
  app_led_set_state(APP_LED_IDLE);
  // No explicit stop needed for RMT RX usually, just ignore next callback or
  // let it finish.
  return ESP_OK;
}

// Check if data was received (polled by main loop or web handler)
bool app_ir_is_data_ready(void) {
  return (!s_is_learning && s_learning_num_symbols >= APP_IR_MIN_SYMBOLS);
}

esp_err_t app_ir_save_learned_result(const char *key) {
  if (s_learning_num_symbols < APP_IR_MIN_SYMBOLS) {
    ESP_LOGW(TAG, "Insufficient IR data symbols (%d) to save for %s",
             (int)s_learning_num_symbols, key);
    return ESP_FAIL;
  }

  // On ESP32-C3/S3, rmt_symbol_word_t is 32-bits and holds 2 symbols (16-bit
  // each).
  uint32_t logical_symbol_count =
      s_learning_num_symbols * (sizeof(rmt_symbol_word_t) / sizeof(uint16_t));

  // Max Encoded Size Estimation:
  // Header (1+4+1) + Palette (16*2) + Data (N/2) + Safety
  size_t max_encoded_size = 6 + 32 + (logical_symbol_count / 2) + 16;

  uint8_t *buffer = (uint8_t *)app_ir_malloc(max_encoded_size);
  if (!buffer) {
    ESP_LOGE(TAG, "Failed to allocate %d bytes for IR encoding",
             (int)max_encoded_size);
    return ESP_ERR_NO_MEM;
  }

  size_t encoded_len = app_ir_encode(s_learning_symbols, logical_symbol_count,
                                     buffer, max_encoded_size);

  if (encoded_len == 0) {
    ESP_LOGE(TAG, "IR Encoding Failed");
    free(buffer);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Saving %s: %" PRIu32 " symbols -> %d bytes", key,
           logical_symbol_count, (int)encoded_len);

  esp_err_t err = app_data_save_ir(key, buffer, encoded_len);
  free(buffer);
  return err;
}

esp_err_t app_ir_send_key(const char *key) {
  if (!s_tx_channel || !s_ir_encoder)
    return ESP_ERR_INVALID_STATE;

  size_t loaded_size = 0;
  // First, get the size
  if (app_data_load_ir(key, NULL, &loaded_size) != ESP_OK || loaded_size == 0) {
    ESP_LOGE(TAG, "Key %s not found or invalid", key);
    return ESP_FAIL;
  }

  uint8_t *buffer = (uint8_t *)app_ir_malloc(loaded_size);
  if (!buffer)
    return ESP_ERR_NO_MEM;

  if (app_data_load_ir(key, buffer, &loaded_size) != ESP_OK) {
    free(buffer);
    return ESP_FAIL;
  }

  // Check Format
  if (buffer[0] != IR_DATA_MAGIC) {
    ESP_LOGE(TAG, "Invalid IR Data Format (Magic mismatch)");
    free(buffer);
    return ESP_FAIL;
  }

  // Get Count (Offset 1, 4 bytes)
  uint32_t num_symbols;
  memcpy(&num_symbols, buffer + 1, 4);

  // Allocate RMT symbols
  // We need to store 'num_symbols' logical symbols (uint16_t)
  // Aligned to 4 bytes
  size_t alloc_size = num_symbols * sizeof(uint16_t);
  if (alloc_size % 4 != 0)
    alloc_size += 2;

  rmt_symbol_word_t *tx_symbols =
      (rmt_symbol_word_t *)app_ir_malloc(alloc_size);

  if (!tx_symbols) {
    free(buffer);
    return ESP_ERR_NO_MEM;
  }

  size_t decoded_count =
      app_ir_decode(buffer, loaded_size, tx_symbols, alloc_size);
  if (decoded_count != num_symbols) {
    ESP_LOGE(TAG, "IR Decode Mismatch (Exp: %" PRIu32 ", Got: %d)", num_symbols,
             (int)decoded_count);
    free(buffer);
    free(tx_symbols);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending %s (%" PRIu32 " symbols)...", key, num_symbols);

  app_led_set_state(APP_LED_IR_TX);

  rmt_transmit_config_t tx_config = {.loop_count = 0};
  ESP_ERROR_CHECK(
      rmt_transmit(s_tx_channel, s_ir_encoder, tx_symbols,
                   num_symbols * sizeof(uint16_t), // Pass BYTES of payload
                   &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_tx_channel, -1));

  app_led_set_state(APP_LED_IDLE);
  free(tx_symbols);
  free(buffer);
  return ESP_OK;
}

esp_err_t app_ir_send_raw(const uint16_t *durations, size_t count) {
  if (!s_tx_channel || !s_ir_encoder)
    return ESP_ERR_INVALID_STATE;

  if (count == 0 || durations == NULL)
    return ESP_ERR_INVALID_ARG;

  // Allocate RMT symbols
  // Allocate RMT symbols
  size_t alloc_size = (count * sizeof(uint16_t));
  if (alloc_size % 4 != 0)
    alloc_size += 2;

  rmt_symbol_word_t *tx_symbols =
      (rmt_symbol_word_t *)app_ir_malloc(alloc_size);
  if (!tx_symbols) {
    ESP_LOGE(TAG, "Failed to allocate memory for %d raw symbols", (int)count);
    return ESP_ERR_NO_MEM;
  }

  uint16_t *tx_raw = (uint16_t *)tx_symbols;

  // Convert uS durations to RMT symbols
  // IRremoteESP8266 format: [puls, space, pulse, space, ...]
  for (size_t i = 0; i < count; i++) {
    uint16_t level = (i % 2 == 0) ? 1 : 0; // Starts with Pulse (Mark)
    uint16_t duration = durations[i];
    tx_raw[i] = duration | (level << 15);
  }

  ESP_LOGI(TAG, "Sending Raw IR Signal (%d pulses/spaces)...", (int)count);
  app_led_set_state(APP_LED_IR_TX);

  rmt_transmit_config_t tx_config = {.loop_count = 0};
  ESP_ERROR_CHECK(rmt_transmit(s_tx_channel, s_ir_encoder, tx_symbols,
                               count * sizeof(uint16_t), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_tx_channel, -1));

  app_led_set_state(APP_LED_IDLE);
  free(tx_symbols);
  return ESP_OK;
}

esp_err_t app_ir_send_cmd(app_ir_cmd_t cmd) {
  return app_ir_send_key((cmd == APP_IR_CMD_AC_ON) ? "ac_on" : "ac_off");
}

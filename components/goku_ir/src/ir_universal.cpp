#include "ir_universal.hpp"
#include "esp_log.h"
#include <cstdlib>
#include <cstring>

static const char *TAG = "goku_ir_universal";

static void fill_pair(uint16_t *raw, size_t *idx, uint16_t mark,
                      uint16_t space) {
  if (!raw)
    return;
  // Mark (Level 1)
  raw[(*idx)++] = mark | 0x8000;
  // Space (Level 0)
  raw[(*idx)++] = space;
}

rmt_symbol_word_t *
ir_universal_generate_symbols(const ir_protocol_config_t *config,
                              const uint8_t *payload, size_t payload_len,
                              size_t *out_size) {
  if (!config || !payload || payload_len == 0 || !out_size) {
    ESP_LOGE(TAG, "Invalid args");
    return NULL;
  }

  // 1. Calculate Estimations
  // Each byte = 8 bits = 8 pairs (16 items)
  // + Header (2 items) + Footer (2 items)
  // * Repeats
  size_t items_per_frame = 2 + (payload_len * 8 * 2) + 2;
  size_t total_items = items_per_frame * (1 + config->frame_repeats);

  // Align to even
  if (total_items % 2 != 0)
    total_items++;

  size_t alloc_bytes = total_items * sizeof(uint16_t);
  // Align to 4 bytes for ESP32 RMT
  if (alloc_bytes % 4 != 0)
    alloc_bytes += 2;

  rmt_symbol_word_t *buffer = (rmt_symbol_word_t *)calloc(1, alloc_bytes);
  if (!buffer) {
    ESP_LOGE(TAG, "No memory");
    return NULL;
  }

  uint16_t *raw = (uint16_t *)buffer;
  size_t idx = 0;

  // 2. Generate Logic
  for (int r = 0; r <= config->frame_repeats; r++) {
    // A. Header
    if (config->header_mark > 0) {
      fill_pair(raw, &idx, config->header_mark, config->header_space);
    }

    // B. Payload
    for (size_t i = 0; i < payload_len; i++) {
      uint8_t byte = payload[i];
      for (int b = 0; b < 8; b++) {
        // Determine bit value based on LSB/MSB
        int bit_idx = config->lsb_first ? b : (7 - b);
        bool bit_val = (byte >> bit_idx) & 1;

        if (bit_val) {
          fill_pair(raw, &idx, config->bit1_mark, config->bit1_space);
        } else {
          fill_pair(raw, &idx, config->bit0_mark, config->bit0_space);
        }
      }
    }

    // C. Footer / Stop Bit
    // If we differ footer from gap
    uint16_t gap = (r < config->frame_repeats) ? config->frame_gap : 0;

    // Normally Footer Mark + Gap Space
    fill_pair(raw, &idx, config->footer_mark,
              gap > 0 ? gap : config->footer_space);
  }

  // Pad if odd number of u16 items (to match u32 RMT word)
  if (idx % 2 != 0) {
    raw[idx++] = 0;
  }

  *out_size = idx / 2; // Return words
  return buffer;
}

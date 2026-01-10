#include "ir_protocol_samsung.hpp"
#include "driver/rmt_types.h" // Required for rmt_symbol_word_t
#include "esp_log.h"
#include <cstdlib>
#include <cstring>

static const char *TAG = "Samsung";

// Timings (Reverse Engineered from User Capture)
// Header: 4.4ms / 4.4ms
// Bit 1: 560us / 1600us
// Bit 0: 560us / 560us
// Frame Gap: ~5.2ms
#define SAMSUNG_HEADER_MARK 4400
#define SAMSUNG_HEADER_SPACE 4400
#define SAMSUNG_BIT_MARK 560
#define SAMSUNG_ONE_SPACE 1600
#define SAMSUNG_ZERO_SPACE 560
#define SAMSUNG_FRAME_GAP 5200

static void fill_pair(uint16_t *raw, size_t *idx, uint16_t mark,
                      uint16_t space) {
  if (!raw)
    return;
  // item 0: Mark (Level 1)
  raw[(*idx)++] = mark | 0x8000;
  // item 1: Space (Level 0)
  raw[(*idx)++] = space;
}

rmt_symbol_word_t *ir_samsung_generate_symbols(const ir_ac_state_t *state,
                                               size_t *out_size) {
  // 1. Prepare Payload (6 Bytes)
  uint8_t payload[6];

  // Byte 0-1: Signature (Fixed 0x4D)
  payload[0] = 0x4D;
  payload[1] = 0xB2; // ~0x4D

  // Byte 2-3: Data1 Pair
  // ON:  Byte 3 = 0x02, Byte 2 = ~0x02 = 0xFD
  // OFF: Byte 3 = 0x21, Byte 2 = ~0x21 = 0xDE

  // Byte 4-5: Data2 Pair
  // ON:  Byte 4 = Temp - 22, Byte 5 = ~Byte4
  // OFF: Byte 4 = 0x07 ? (Fixed), Byte 5 = ~0x07 = 0xF8

  uint8_t data1, data2;

  if (state->power) {
    // ON State
    data1 = 0x02; // Mode (Auto/Cool)

    // Temperature Mapping (Non-linear / Obfuscated)
    // Verified from Capture:
    // 24C -> 0x02
    // 25C -> 0x03
    // Temperature Mapping (Fully Decoded)
    // Captured from user remote.
    // Logic: Gray-coded variation, with some outliers (e.g. 21C=0x83).
    uint8_t t_map[15] = {
        0x00, // 16C (Mapped to 17C)
        0x00, // 17C
        0x08, // 18C
        0x0C, // 19C
        0x04, // 20C
        0x83, // 21C (Verified: 83+7C=FF)
        0x0E, // 22C
        0x0A, // 23C
        0x02, // 24C
        0x03, // 25C
        0x0B, // 26C
        0x09, // 27C
        0x01, // 28C
        0x05, // 29C
        0x0D  // 30C
    };

    int idx = (int)state->temp - 16;
    if (idx < 0)
      idx = 0;
    if (idx > 14)
      idx = 14;

    data2 = t_map[idx];

  } else {
    // OFF State (Fixed Payload from Capture)
    data1 = 0x21; // OFF Command
    data2 = 0x07; // OFF Fixed param
  }

  // Fill Payload
  payload[2] = ~data1;
  payload[3] = data1;
  payload[4] = data2;
  payload[5] = ~data2;

  ESP_LOGI(
      TAG, "Sending Samsung (Legacy 48-bit): %02X %02X %02X %02X %02X %02X",
      payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]);

  // 2. Allocate Buffer
  // 2 Frames. Each frame: Header(2 items) + 48 bits * 2 items + Footer(2 items)
  // = ~100 items per frame Total ~200 items (u16). Allocate generous buffer.
  size_t alloc_items = 256;
  // Allocate bytes = items * 2
  size_t alloc_bytes = alloc_items * sizeof(uint16_t);
  // Align to 4 bytes for rmt_symbol_word_t
  if (alloc_bytes % 4 != 0)
    alloc_bytes += 2;

  rmt_symbol_word_t *buffer = (rmt_symbol_word_t *)calloc(1, alloc_bytes);
  if (!buffer)
    return NULL;

  uint16_t *raw = (uint16_t *)buffer;
  size_t idx = 0;

  // 3. Generate Symbols (2 Frames)
  for (int frame = 0; frame < 2; frame++) {
    // Header
    fill_pair(raw, &idx, SAMSUNG_HEADER_MARK, SAMSUNG_HEADER_SPACE);

    // Data (48 bits / 6 bytes)
    for (int i = 0; i < 6; i++) {
      for (int b = 0; b < 8; b++) {
        // LSB First
        bool bit = (payload[i] >> b) & 1;
        fill_pair(raw, &idx, SAMSUNG_BIT_MARK,
                  bit ? SAMSUNG_ONE_SPACE : SAMSUNG_ZERO_SPACE);
      }
    }

    // Footer / Gap
    uint16_t gap = (frame == 0) ? SAMSUNG_FRAME_GAP
                                : SAMSUNG_ZERO_SPACE; // Last space is just idle
    fill_pair(raw, &idx, SAMSUNG_BIT_MARK, gap);
  }

  // Ensure even number of 16-bit items for 32-bit alignment if strict
  if (idx % 2 != 0) {
    raw[idx++] = 0; // Pad
  }

  *out_size = idx / 2; // Return count of WORDS (32-bit)
  return buffer;
}

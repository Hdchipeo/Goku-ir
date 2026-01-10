#include "ir_protocol_daikin.hpp"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

// Daikin Timing
// Based on standard Daikin 19-byte or similar protocols
// Header: 3.5ms Mark, 1.75ms Space
// Bit 1: 430us Mark, 1300us Space
// Bit 0: 430us Mark, 430us Space
// Connectors (gap between frames): same as start? or pause?
// Daikin frames usually:
// Frame 1: Header + 8 bytes + Stop
// Pause (~20-30ms)
// Frame 2: Header + 19 bytes + Stop (Main control) // Sometimes 3 frames

// Let's implement common Daikin ARC433 series (most popular)
// Frame 1: 8 Bytes (Header info)
// Frame 2: 19 Bytes (Control info)

#define DAIKIN_HDR_MARK 3500
#define DAIKIN_HDR_SPACE 1750
#define DAIKIN_BIT_MARK 430
#define DAIKIN_ONE_SPACE 1300
#define DAIKIN_ZERO_SPACE 430
#define DAIKIN_IGNORE_HEADER_MARK 430 // Connectors often start with this

// Helper for adding logic
#define ADD_SYMBOL(d, l)                                                       \
  do {                                                                         \
    if (idx < max_words * 2) {                                                 \
      raw[idx++] = (d) | ((l) << 15);                                          \
    }                                                                          \
  } while (0)

static void encode_byte(uint16_t *raw, int *idx_ptr, uint8_t data,
                        size_t max_words) {
  int idx = *idx_ptr;
  for (int i = 0; i < 8; i++) {
    ADD_SYMBOL(DAIKIN_BIT_MARK, 1);
    if ((data >> i) & 1) { // LSB First
      ADD_SYMBOL(DAIKIN_ONE_SPACE, 0);
    } else {
      ADD_SYMBOL(DAIKIN_ZERO_SPACE, 0);
    }
  }
  *idx_ptr = idx;
}

static uint8_t calc_checksum(const uint8_t *data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += data[i];
  return sum;
}

// Daikin ARC433 Structure
// Frame 1 (8B): 11 DA 27 00 C5 00 00 D7
// Frame 2 (19B): 11 DA 27 00 42 [Mode/Temp] ... [Checksum]

rmt_symbol_word_t *ir_daikin_generate_symbols(const ir_ac_state_t *state,
                                              size_t *out_size) {
  // 1. Prepare Data Buffers
  uint8_t frame1[8] = {0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, 0x00, 0xD7};
  uint8_t frame2[19] = {0x11, 0xDA, 0x27, 0x00, 0x42, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x06, 0x00};

  // Apply State to Frame 2
  // Encode Power (Bit 0 of Byte 5)
  if (state->power)
    frame2[5] |= 0x01;
  else
    frame2[5] &= ~0x01;

  // Encode Mode (Byte 5 high nibble?)
  // 0:Auto, 1:Cool, 2:Heat, 3:Fan, 4:Dry
  // Daikin Mode Map:
  // Auto=0x00? Cool=0x30, Heat=0x40, Fan=0x60, Dry=0x20. (Valid logical
  // guess/reference needed) Using mapping from IRremoteESP8266 or generic
  // knowledge: Cool: 0x3, Heat: 0x4, Fan: 0x6, Auto: 0x0/0x1/0x7, Dry: 0x2
  // Place in Byte 5 upper nibble usually?
  // Actually, Byte 5: [Mode:4][OffTimer:3][Power:1]

  uint8_t mode_val = 0x3; // Default Cool
  switch (state->mode) {
  case 0:
    mode_val = 0x0;
    break; // Auto
  case 1:
    mode_val = 0x3;
    break; // Cool
  case 2:
    mode_val = 0x4;
    break; // Heat
  case 3:
    mode_val = 0x6;
    break; // Fan
  case 4:
    mode_val = 0x2;
    break; // Dry
  default:
    mode_val = 0x3;
  }
  frame2[5] |= (mode_val << 4);

  // Encode Temp (Byte 6)
  // Val = (Temp - 10) * 2? Or just raw?
  // Daikin: (Temp * 2)
  frame2[6] = (state->temp * 2);

  // Checksum
  frame2[18] = calc_checksum(frame2, 18);

  // 2. Allocate Buffer
  // Frame 1: Header(2) + 8*16 + Stop(1) + Gap(1) = 132
  // Frame 2: Header(2) + 19*16 + Stop(1) = 307
  // Total approx 440 items -> 220 words
  size_t max_words = 300;
  rmt_symbol_word_t *buffer =
      (rmt_symbol_word_t *)calloc(max_words, sizeof(rmt_symbol_word_t));
  if (!buffer)
    return NULL;

  uint16_t *raw = (uint16_t *)buffer;
  int idx = 0;

  // --- Frame 1 ---
  ADD_SYMBOL(DAIKIN_HDR_MARK, 1);
  ADD_SYMBOL(DAIKIN_HDR_SPACE, 0);

  for (int i = 0; i < 8; i++)
    encode_byte(raw, &idx, frame1[i], max_words);

  ADD_SYMBOL(DAIKIN_BIT_MARK, 1); // Stop Mark
  ADD_SYMBOL(25000, 0);           // Gap (25ms)

  // --- Frame 2 ---
  ADD_SYMBOL(DAIKIN_HDR_MARK, 1);
  ADD_SYMBOL(DAIKIN_HDR_SPACE, 0);

  for (int i = 0; i < 19; i++)
    encode_byte(raw, &idx, frame2[i], max_words);

  ADD_SYMBOL(DAIKIN_BIT_MARK, 1); // Stop Mark

  // Safety Pad
  if (idx % 2 != 0)
    ADD_SYMBOL(0, 0);

  *out_size = idx / 2;
  return buffer;
}

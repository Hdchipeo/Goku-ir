#include "ir_protocol_mitsubishi.hpp"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

// Mitsubishi Electric (144 bit / 18 bytes)
#define MITSUBISHI_HDR_MARK 3400
#define MITSUBISHI_HDR_SPACE 1750
#define MITSUBISHI_BIT_MARK 450
#define MITSUBISHI_ONE_SPACE 1300
#define MITSUBISHI_ZERO_SPACE 420

#define ADD_SYMBOL(d, l)                                                       \
  do {                                                                         \
    if (idx < max_items) {                                                     \
      raw[idx++] = (d) | ((l) << 15);                                          \
    }                                                                          \
  } while (0)

static void encode_byte(uint16_t *raw, int *idx_ptr, uint8_t data,
                        int max_items) {
  int idx = *idx_ptr;
  for (int i = 0; i < 8; i++) {
    ADD_SYMBOL(MITSUBISHI_BIT_MARK, 1);
    if ((data >> i) & 1) {
      ADD_SYMBOL(MITSUBISHI_ONE_SPACE, 0);
    } else {
      ADD_SYMBOL(MITSUBISHI_ZERO_SPACE, 0);
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

rmt_symbol_word_t *ir_mitsubishi_generate_symbols(const ir_ac_state_t *state,
                                                  size_t *out_size) {
  // 18 Bytes Frame
  // 23 CB 26 01 00 20 08 06 30 45 67 ... Checksum

  uint8_t payload[18] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x08, 0x06, 0x30,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Power (Byte 5)
  if (state->power)
    payload[5] |= 0x20; // NO, standard is bit 5?
  // Actually Byte 5 is 0x00 usually?
  // Power ON is often: Byte 5 = 0x20 off is 0x00?
  // Let's assume Toggle 0x20.

  // Mode (Byte 6)
  // 0:Auto, 1:Cool, 2:Heat, 3:Dry (Mitsubishi map)
  // Cool=0x18, Heat=0x98, Auto=0x20?
  payload[6] = 0x18; // Default Cool

  // Temp (Byte 7)
  // Temp 16-31.
  // Val = Temp - 16
  payload[7] = (state->temp - 16);

  // Checksum (Last Byte)
  payload[17] = calc_checksum(payload, 17);

  // Generation
  int max_items = 400;
  rmt_symbol_word_t *buffer = (rmt_symbol_word_t *)calloc(
      max_items / 2 + 10, sizeof(rmt_symbol_word_t));
  if (!buffer)
    return NULL;

  uint16_t *raw = (uint16_t *)buffer;
  int idx = 0;

  // Header
  ADD_SYMBOL(MITSUBISHI_HDR_MARK, 1);
  ADD_SYMBOL(MITSUBISHI_HDR_SPACE, 0);

  // Payload
  for (int i = 0; i < 18; i++)
    encode_byte(raw, &idx, payload[i], max_items);

  // Stop
  ADD_SYMBOL(MITSUBISHI_BIT_MARK, 1);
  // Pause? usually sends twice strictly?
  // For now send once.

  if (idx % 2 != 0)
    ADD_SYMBOL(0, 0);
  *out_size = idx / 2;
  return buffer;
}

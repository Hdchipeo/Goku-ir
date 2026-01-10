#include "ir_protocol_nec.hpp"
#include <stdlib.h>
#include <string.h>

// NEC Timings (microseconds)
#define NEC_HEADER_MARK 9000
#define NEC_HEADER_SPACE 4500
#define NEC_BIT_MARK 560
#define NEC_ONE_SPACE 1690
#define NEC_ZERO_SPACE 560
#define NEC_STOP_BIT 560

rmt_symbol_word_t *ir_nec_generate_symbols(uint16_t address, uint16_t command,
                                           size_t *out_size) {
  // 32-bit payload + 1 Header Mark/Space + 1 Stop Mark = 34 transitions?
  // NEC Frame:
  // Header Mark (9ms), Header Space (4.5ms) = 2 items
  // 32 bits * 2 items (Mark + Space) = 64 items
  // Stop Mark (560us) = 1 item
  // Total = 67 items
  // Start + 2 + 64 + 1 = 67.

  // RMT hardware on S3 expects 16-bit values packed into 32-bit words.
  // We treat the buffer as array of uint16_t for simplicity.
  // 67 items -> 68 uint16_t (aligned to 4 bytes / 2 items) -> 34 words.

  size_t num_items = 2 + 32 * 2 + 1;
  if (num_items % 2 != 0)
    num_items++; // Align to even number for full 32-bit words

  size_t buffer_size_bytes = num_items * sizeof(uint16_t);
  rmt_symbol_word_t *buffer = (rmt_symbol_word_t *)calloc(1, buffer_size_bytes);
  if (!buffer)
    return NULL;

  uint16_t *raw = (uint16_t *)buffer;
  int idx = 0;

// Helper macro to add symbol
// Level is bit 15, Duration is 0-14
#define ADD_ITEM(dur, lvl) raw[idx++] = (dur) | ((lvl) << 15)

  // 1. Header
  ADD_ITEM(NEC_HEADER_MARK, 1);
  ADD_ITEM(NEC_HEADER_SPACE, 0);

  // 2. Payload
  // Format: Address Low -> Address High -> Command -> ~Command
  uint8_t bytes[4];
  bytes[0] = address & 0xFF;
  bytes[1] = (address >> 8) & 0xFF;
  bytes[2] = command & 0xFF;
  bytes[3] = ~(command & 0xFF);

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 8; j++) {
      bool bit = (bytes[i] >> j) & 1;
      uint16_t space = bit ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
      ADD_ITEM(NEC_BIT_MARK, 1);
      ADD_ITEM(space, 0);
    }
  }

  // 3. Stop
  ADD_ITEM(NEC_STOP_BIT, 1);

  // 4. Pad if needed
  if (idx % 2 != 0) {
    ADD_ITEM(0, 0); // Zero duration, level 0
  }

  // Output size is number of SYMBOLS (logical items? or Words?)
  // rmt_transmit expects "size in bytes" or "number of symbols"?
  // In `app_ir.c`: `count * sizeof(uint16_t)` is passed to rmt_transmit.
  // So `ir_engine_send_raw` takes `count` as number of uint16_t items?
  // Wait, `ir_engine_send_raw` takes `rmt_symbol_word_t*` and `count`
  // (symbols). In `ir_rmt.cpp`: `count * sizeof(rmt_symbol_word_t)`. Wait!
  // `rmt_symbol_word_t` is 4 bytes. If I pass `count` as "number of 16-bit
  // items", then `count * 4` is WRONG. I should standardize `count` to mean
  // "Number of rmt_symbol_word_t (32-bit)".

  // In `ir_nec_generate_symbols`, I return `buffer` which is
  // `rmt_symbol_word_t*`. `out_size` should be number of *words*.

  *out_size = num_items / 2; // num_items is even now.

  return buffer;
}

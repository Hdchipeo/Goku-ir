#pragma once

#include "driver/rmt_types.h"
#include "ir_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate RMT symbols for ANY protocol defined by config.
 *
 * @param config Pointer to the protocol timing configuration.
 * @param payload Pointer to the raw data bytes to send.
 * @param payload_len Length of the payload in bytes.
 * @param out_size [Out] Number of 32-bit RMT words generated.
 * @return rmt_symbol_word_t* Pointer to allocated buffer (Must be free() by
 * caller).
 */
rmt_symbol_word_t *
ir_universal_generate_symbols(const ir_protocol_config_t *config,
                              const uint8_t *payload, size_t payload_len,
                              size_t *out_size);

#ifdef __cplusplus
}
#endif

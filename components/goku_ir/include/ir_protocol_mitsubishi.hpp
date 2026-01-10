#pragma once

#include "driver/rmt_types.h"
#include "ir_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate RMT symbols for Mitsubishi AC.
 *
 * @param state AC State
 * @param[out] out_size Number of RMT words
 * @return rmt_symbol_word_t*
 */
rmt_symbol_word_t *ir_mitsubishi_generate_symbols(const ir_ac_state_t *state,
                                                  size_t *out_size);

#ifdef __cplusplus
}
#endif

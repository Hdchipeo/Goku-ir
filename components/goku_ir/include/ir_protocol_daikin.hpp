#pragma once

#include "driver/rmt_types.h"
#include "ir_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate RMT symbols for Daikin AC based on state.
 *        Caller must free the returned buffer.
 *
 * @param state AC State (Power, Mode, Temp, Fan, Swing)
 * @param[out] out_size Number of RMT words generated
 * @return rmt_symbol_word_t* Pointer to allocated buffer
 */
rmt_symbol_word_t *ir_daikin_generate_symbols(const ir_ac_state_t *state,
                                              size_t *out_size);

#ifdef __cplusplus
}
#endif

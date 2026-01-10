#pragma once

#include "driver/rmt_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate RMT symbols for a NEC command.
 *        Caller must free the returned buffer.
 *
 * @param address 16-bit Address
 * @param command 16-bit Command
 * @param[out] out_size Number of symbols generated
 * @return rmt_symbol_word_t* Pointer to allocated buffer (or NULL on error)
 */
rmt_symbol_word_t *ir_nec_generate_symbols(uint16_t address, uint16_t command,
                                           size_t *out_size);

#ifdef __cplusplus
}
#endif

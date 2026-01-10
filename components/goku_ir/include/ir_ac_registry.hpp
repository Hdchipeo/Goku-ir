#pragma once

#include "ir_types.h"
#include "ir_universal.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Function pointer to translate Generic State -> Raw Payload Bytes
typedef void (*ac_translator_func_t)(const ir_ac_state_t *state,
                                     uint8_t *out_payload, size_t *out_len);

// Entry in the Registry
typedef struct {
  ac_brand_t brand_id;
  ir_protocol_config_t protocol;
  ac_translator_func_t translator;
} ir_ac_definition_t;

/**
 * @brief Find an AC definition by Brand ID
 *
 * @param brand Brand Enum
 * @return const ir_ac_definition_t* or NULL if not found
 */
const ir_ac_definition_t *ir_ac_registry_get(ac_brand_t brand);

#ifdef __cplusplus
}
#endif

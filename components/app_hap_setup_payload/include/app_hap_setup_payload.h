#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_hap_setup_payload(char *setup_code, char *setup_id, bool paired,
                           uint8_t category);

#ifdef __cplusplus
}
#endif

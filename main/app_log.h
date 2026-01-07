#pragma once

#include "esp_err.h"
#include <stddef.h>

void app_log_init(void);
int app_log_get_buffer(char *dest, size_t max_len);
void app_log_clear(void);

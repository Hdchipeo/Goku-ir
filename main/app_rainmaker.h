#pragma once

#include <esp_err.h>
#include <stdbool.h>

/**
 * Initialize ESP RainMaker functionality.
 * This handles provisioning, connection, and creating the AC device node.
 */
esp_err_t app_rainmaker_init(void);

/**
 * Updates the RainMaker state when local changes occur (e.g. physical buttons)
 */
void app_rainmaker_update_state(bool power_on);

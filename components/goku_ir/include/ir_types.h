#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Generic AC State
typedef enum {
  IR_AC_MODE_AUTO = 0,
  IR_AC_MODE_COOL = 1,
  IR_AC_MODE_HEAT = 2,
  IR_AC_MODE_FAN = 3,
  IR_AC_MODE_DRY = 4
} ir_ac_mode_t;

typedef enum {
  IR_AC_FAN_AUTO = 0,
  IR_AC_FAN_LOW = 1,
  IR_AC_FAN_MED = 2,
  IR_AC_FAN_HIGH = 3,
  IR_AC_FAN_TURBO = 4
} ir_ac_fan_t;

typedef enum {
  AC_BRAND_DAIKIN = 0,
  AC_BRAND_SAMSUNG,
  AC_BRAND_MITSUBISHI,
  AC_BRAND_PANASONIC,
  AC_BRAND_LG,
  AC_BRAND_MAX
} ac_brand_t;

typedef struct {
  bool power;
  uint8_t temp;    // 16-30
  uint8_t mode;    // See ir_ac_mode_t
  uint8_t fan;     // See ir_ac_fan_t
  uint8_t swing_v; // Vertical Swing
  uint8_t swing_h; // Horizontal Swing
} ir_ac_state_t;

/**
 * @brief Universal Protocol Configuration (Data-Driven)
 * Defines the physical layer timings for a Pulse-Distance/Pulse-Width protocol.
 */
typedef struct {
  const char *name;
  uint32_t carrier_freq; // Hz, e.g. 38000
  uint8_t duty_cycle;    // Percent, e.g. 33

  // Header
  uint16_t header_mark;
  uint16_t header_space;

  // Bit 1 (Logic High)
  uint16_t bit1_mark;
  uint16_t bit1_space;

  // Bit 0 (Logic Low)
  uint16_t bit0_mark;
  uint16_t bit0_space;

  // Footer / Stop Bit
  uint16_t footer_mark;
  uint16_t footer_space;

  // Formatting
  bool lsb_first;        // true = LSB, false = MSB
  uint16_t frame_gap;    // Space between repeats (us)
  uint8_t frame_repeats; // 0 = send once, 1 = send twice (repeat once)...
} ir_protocol_config_t;

typedef void (*ir_rx_callback_t)(uint16_t addr, uint16_t cmd, void *ctx);

// Context for RMT TX
typedef struct {
  int gpio_num;
  int resolution_hz;
} ir_engine_config_t;

#ifdef __cplusplus
}
#endif

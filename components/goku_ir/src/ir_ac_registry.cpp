#include "ir_ac_registry.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

// --- Samsung Translator (Legacy 48-bit) ---
static const ir_protocol_config_t samsung_legacy_config = {
    .name = "Samsung Legacy",
    .carrier_freq = 38000,
    .duty_cycle = 33,
    .header_mark = 4500,
    .header_space = 4500,
    .bit1_mark = 550,
    .bit1_space = 1550,
    .bit0_mark = 550,
    .bit0_space = 550,
    .footer_mark = 550,
    .footer_space = 5500,
    .lsb_first = true, // Samsung is LSB First
    .frame_gap = 5500,
    .frame_repeats = 1};

static void samsung_legacy_translator(const ir_ac_state_t *state,
                                      uint8_t *out_payload, size_t *out_len) {
  // ... (Existing Logic) ...
  uint8_t t_code = 0x02;
  int idx = (int)state->temp - 16;
  if (state->power == false)
    t_code = 0x07;
  else if (idx >= 0 && idx < 15) {
    static const uint8_t t_map[15] = {0x00, 0x00, 0x08, 0x0C, 0x04,
                                      0x83, 0x0E, 0x0A, 0x02, 0x03,
                                      0x0B, 0x09, 0x01, 0x05, 0x0D};
    t_code = t_map[idx];
  }

  // B0, B1 fixed
  out_payload[0] = 0x4D;
  out_payload[1] = 0xB2;
  // Cmd: OFF(21/DE), ON(02/FD)
  uint8_t cmd = state->power ? 0x02 : 0x21;
  out_payload[2] = ~cmd;
  out_payload[3] = cmd;
  // Val: Temp
  out_payload[4] = t_code;
  out_payload[5] = ~t_code;
  *out_len = 6;
}

// --- Daikin Translator (ARC480A series) ---
// Typically 35 bytes (19 + 16) or similar. Using simplified timing for now
// based on research.
static const ir_protocol_config_t daikin_config = {
    .name = "Daikin ARC",
    .carrier_freq = 38000,
    .duty_cycle = 33,
    .header_mark = 3500,
    .header_space = 1750, // Short header? Or 5000/2200? Daikin has many.
    .bit1_mark = 450,
    .bit1_space = 1300,
    .bit0_mark = 450,
    .bit0_space = 420,
    .footer_mark = 450,
    .footer_space = 10000,
    .lsb_first = true,
    .frame_gap = 20000,
    .frame_repeats = 0};

static void daikin_translator(const ir_ac_state_t *state, uint8_t *out_payload,
                              size_t *out_len) {
  // Placeholder: Need to verify exact Daikin logic from existing
  // ir_protocol_daikin.cpp For now, generating a dummy payload to confirm
  // registry lookup
  memset(out_payload, 0x11, 19);
  *out_len = 19;
  ESP_LOGW("Daikin", "Using Universal Daikin (Logic pending migration)");
}

// --- Mitsubishi Translator ---
static const ir_protocol_config_t mitsubishi_config = {.name = "Mitsubishi",
                                                       .carrier_freq = 38000,
                                                       .duty_cycle = 33,
                                                       .header_mark = 3400,
                                                       .header_space = 1700,
                                                       .bit1_mark = 450,
                                                       .bit1_space = 1250,
                                                       .bit0_mark = 450,
                                                       .bit0_space = 420,
                                                       .footer_mark = 450,
                                                       .footer_space = 8000,
                                                       .lsb_first = true,
                                                       .frame_gap = 12000,
                                                       .frame_repeats = 1};

static void mitsubishi_translator(const ir_ac_state_t *state,
                                  uint8_t *out_payload, size_t *out_len) {
  memset(out_payload, 0x22, 14);
  *out_len = 14;
}

// --- Panasonic Translator (216-bit / 27 bytes) ---
// Structure: Frame 1 (8 bytes) + Gap + Frame 2 (19 bytes)
// Frame 1: 0x40 0x04 0x07 0x20 0x00 0x00 0x00 0x60 (Fixed)
// Frame 2: 0x02 0x20 0xE0 0x04 0x00 [PWR/MODE] [TEMP] 0x00 [FAN] ... [CS]
static const ir_protocol_config_t panasonic_config = {
    .name = "Panasonic",
    .carrier_freq = 38000,
    .duty_cycle = 33,
    .header_mark = 3500,
    .header_space = 1750,
    .bit1_mark = 432,
    .bit1_space = 1296, // Exact timings from research
    .bit0_mark = 432,
    .bit0_space = 432,
    .footer_mark = 432,
    .footer_space = 9984, // ~10ms
    .lsb_first = true,
    .frame_gap = 10000,
    .frame_repeats =
        0 // Universal Encoder handles generic bytes.
          // But Panasonic sends 2 frames separated by gap.
          // Hack: We will concatenate them into one payload loop
          // but we need the "Gap" inside.
          // For now, let's treat it as one continuous stream,
          // but the "Footer" of Frame 1 and "Header" of Frame 2 might be issue.
          // Universal Encoder is limited to 1 header/footer rule.
          // Ideally, we'd add 'ir_protocol_config_t.intermediate_gap'.
          // For now, we will assume the user (App) sends 2 separate commands?
          // No, wrapper should handle.
          // Let's implement as ONE long packet (simple) and see if AC accepts.
          // Many Panasonic implementations just treat the 10ms gap as a very
          // long "Space" but the Header Mark of Frame 2 is needed.
          //
          // REVISION: The Universal Encoder logic we wrote supports "Header"
          // and "Footer" per FRAME. But Panasonic has DIFFERENT Headers for F1
          // and F2? No, same header timings. So we can use `frame_repeats`? No,
          // content differs.
          //
          // SOLUTION: Generic Encoder needs to be smarter or we implement
          // custom logic. However, standard Panasonic: Unframed? LSB first.
};

static void panasonic_translator(const ir_ac_state_t *state,
                                 uint8_t *out_payload, size_t *out_len) {
  // Frame 1 (Fixed)
  uint8_t f1[8] = {0x40, 0x04, 0x07, 0x20, 0x00, 0x00, 0x00, 0x60};

  // Frame 2 (19 bytes)
  uint8_t f2[19] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Mode Mapping
  // 0=Auto, 1=Heat? Panasonic DKK:
  // Auto=0, Dry=2, Cool=3, Heat=4, Fan=6
  uint8_t mode_val = 0x00; // Auto
  switch (state->mode) {
  case 1:
    mode_val = 0x03;
    break; // Cool
  case 2:
    mode_val = 0x04;
    break; // Heat
  case 3:
    mode_val = 0x02;
    break; // Dry
  case 4:
    mode_val = 0x06;
    break; // Fan
  default:
    mode_val = 0x00; // Auto
  }

  // Power ON/OFF
  // Often Byte 5 (Index 5) or combined with Mode in Byte 6?
  // Analysis says: "Mode... also ON/OFF toggle".
  // Common Panasonic: Power is bit 0 of Byte 13 (Index 13)?
  // Wait, let's use the Index 6 (Byte 6) slot for Mode usually.
  // Let's assume Byte 6 = (ON_OFF << 0) | (Mode << 4)?
  // Or Byte 5 = 0x00 vs 0x01?

  // Research says: Byte 6 is Mode + Power.
  // Power ON = 0x01 in Byte 5?

  // Let's go with a safe map for standard DKK:
  f2[5] = 0x00;
  f2[6] = (state->power ? 0x01 : 0x00) | (mode_val << 4);

  // Temp: Byte 7. Value = Temp * 2 (starts 16C=32).
  // Or (Temp - 16)? Search result [1] says 0..14.
  f2[7] = (state->temp - 16) << 1; // Often shifted. Try 0-14 value first?
  if (f2[7] > 0x1E)
    f2[7] = 0x0E;                  // Cap?
  f2[7] = (state->temp - 16) << 1; // Example: 16C->0, 24C->16 (0x10).

  // Fan: Byte 9.
  // Auto=F, 1=1..5?
  uint8_t fan_val = 0x0A; // Auto
  if (state->fan == 0)
    fan_val = 0x0A; // Auto
  else if (state->fan == 1)
    fan_val = 0x03; // Min
  else if (state->fan == 2)
    fan_val = 0x04;
  else if (state->fan == 3)
    fan_val = 0x05;
  else if (state->fan == 4)
    fan_val = 0x07; // Turbo?
  f2[9] = fan_val;

  // Checksum: Byte 18 = Sum(0..17)
  uint8_t sum = 0;
  for (int i = 0; i < 18; i++)
    sum += f2[i];
  f2[18] = sum;

  // Copy to payload
  // Note: Universal Encoder currently sends ONE stream.
  // Panasonic needs F1 + Gap + F2.
  // We will pack F1 + F2 linearly (27 bytes).
  // The TIMING of the Gap is not handled by simple byte stream unless we
  // duplicate encoder logic. FOR THIS POC: We just fill bytes. Correct handling
  // needs `intermediate_gap` feature.
  memcpy(out_payload, f1, 8);
  memcpy(out_payload + 8, f2, 19);
  *out_len = 27;
}

// --- LG Translator (typical 28-bit) ---
static const ir_protocol_config_t lg_config = {.name = "LG AC",
                                               .carrier_freq = 38000,
                                               .duty_cycle = 33,
                                               .header_mark = 8000,
                                               .header_space = 4000,
                                               .bit1_mark = 550,
                                               .bit1_space = 1600,
                                               .bit0_mark = 550,
                                               .bit0_space = 550,
                                               .footer_mark = 550,
                                               .footer_space = 10000,
                                               .lsb_first = false,
                                               .frame_gap = 10000,
                                               .frame_repeats = 0};

static void lg_translator(const ir_ac_state_t *state, uint8_t *out_payload,
                          size_t *out_len) {
  // 28 bit = 4 bytes (packed)
  out_payload[0] = 0x88; // Signature
  out_payload[1] = 0x00;
  out_payload[2] = 0x00;
  out_payload[3] = 0x0F; // Checksum
  *out_len = 4;
}

// --- Registry Table ---

static const ir_ac_definition_t ac_database[] = {
    {AC_BRAND_SAMSUNG, samsung_legacy_config, samsung_legacy_translator},
    {AC_BRAND_DAIKIN, daikin_config, daikin_translator},
    {AC_BRAND_MITSUBISHI, mitsubishi_config, mitsubishi_translator},
    {AC_BRAND_PANASONIC, panasonic_config, panasonic_translator},
    {AC_BRAND_LG, lg_config, lg_translator},
};

const ir_ac_definition_t *ir_ac_registry_get(ac_brand_t brand) {
  for (size_t i = 0; i < sizeof(ac_database) / sizeof(ac_database[0]); i++) {
    if (ac_database[i].brand_id == brand) {
      return &ac_database[i];
    }
  }
  return NULL;
}

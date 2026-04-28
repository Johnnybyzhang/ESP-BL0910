#pragma once
#include <cstdint>

namespace esphome {
namespace bl0906 {

// ADC full-scale RMS voltage at the input pin (PGA=1).
// Derived from the original ESPHome BL0906 constants: 1.097 * 2^22 / (13162 * 1000) ≈ 0.3496V
static constexpr float BL0906_ADC_RMS_FULL_SCALE_VOLTS = 0.3496f;
static constexpr float BL0906_RMS_DIVISOR = static_cast<float>(1 << 22);

// UART timing constraints (BL0906 datasheet section 5.2.3)
static constexpr uint32_t BL0906_T3_WAIT_US = 200;              // t3: min 110us addr->response
static constexpr uint32_t BL0906_RX_START_TIMEOUT_US = 5000;     // max wait for first RX byte
static constexpr uint32_t BL0906_RX_INTERBYTE_TIMEOUT_US = 2000; // max gap between RX bytes
static constexpr size_t BL0906_CAPTURE_MAX = 10;                 // echo (0-2) + response (4) + margin
static constexpr size_t BL0906_FRAME_SIZE = 4;                   // response: l, m, h, checksum

// Retry count for register operations
static constexpr uint8_t BL0906_DEFAULT_MAX_READ_ATTEMPTS = 5;
static constexpr uint8_t BL0906_DEFAULT_IMMEDIATE_READ_ATTEMPTS = 3;
static constexpr uint16_t BL0906_DEFAULT_RETRY_BACKOFF_BASE_MS = 2;
static constexpr uint8_t BL0906_DEFAULT_RETRY_BACKOFF_MULTIPLIER = 2;

}  // namespace bl0906
}  // namespace esphome

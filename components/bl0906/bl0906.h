#pragma once

#include <cmath>
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "constants.h"

namespace esphome {
namespace bl0906 {

static const uint8_t NUM_CHANNELS = 6;

// BL0906 Register Addresses (non-contiguous -- gaps between ch4 and ch5)
// Current RMS registers
static const uint8_t REG_I_RMS[] = {0x0D, 0x0E, 0x0F, 0x10, 0x13, 0x14};
// Voltage RMS register
static const uint8_t REG_V_RMS = 0x16;

// Active power registers (signed)
static const uint8_t REG_WATT[] = {0x23, 0x24, 0x25, 0x26, 0x29, 0x2A};
static const uint8_t REG_WATT_TOTAL = 0x2C;

// Energy pulse count registers (unsigned, 24-bit rollover)
static const uint8_t REG_CF_CNT[] = {0x30, 0x31, 0x32, 0x33, 0x36, 0x37};
static const uint8_t REG_CF_CNT_TOTAL = 0x39;

// Line period (frequency)
static const uint8_t REG_PERIOD = 0x4E;

// Internal temperature
static const uint8_t REG_TPS = 0x5E;

// PGA gain registers
static const uint8_t REG_GAIN1 = 0x60;  // [3:0]=V, [11:8]=Ch1, [15:12]=Ch2, [19:16]=Ch3, [23:20]=Ch4
static const uint8_t REG_GAIN2 = 0x61;  // [11:8]=Ch5, [15:12]=Ch6

// RMS gain trim registers (signed 16-bit, per-input)
static const uint8_t REG_RMSGN[] = {0x6D, 0x6E, 0x6F, 0x70, 0x73, 0x74};  // ch1-6
static const uint8_t REG_RMSGN_V = 0x76;  // voltage

// RMS offset correction registers (signed 24-bit, per-input)
static const uint8_t REG_RMSOS[] = {0x78, 0x79, 0x7A, 0x7B, 0x7E, 0x7F};  // ch1-6
static const uint8_t REG_RMSOS_V = 0x81;  // voltage

// Active power gain registers (signed 16-bit, per-channel)
static const uint8_t REG_WATTGN[] = {0xB7, 0xB8, 0xB9, 0xBA, 0xBD, 0xBE};

// Energy pulse divisor
static const uint8_t REG_CFDIV = 0xCE;

// Mode register
static const uint8_t REG_MODE = 0x98;

// Write protection / reset
static const uint8_t REG_USR_WRPROT = 0x9E;
static const uint8_t REG_SOFT_RESET = 0x9F;

// Protocol commands
static const uint8_t UART_READ_CMD = 0x35;
static const uint8_t UART_WRITE_CMD = 0xCA;

// Special values
static const uint32_t USR_WRPROT_UNLOCK = 0x5555;
static const uint32_t SOFT_RESET_VALUE = 0x5A5A5A;

struct FrontendConfig {
  float load_res{0.0f};
  float sample_res{1.0f};
  float sample_ratio{1.0f};
  uint8_t pga_gain{1};
};

class BL0906Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Frontend configuration setters
  void set_voltage_load_res(float value) { this->voltage_frontend_.load_res = value; }
  void set_voltage_sample_res(float value) { this->voltage_frontend_.sample_res = value; }
  void set_voltage_sample_ratio(float value) { this->voltage_frontend_.sample_ratio = value; }
  void set_voltage_pga_gain(uint8_t value) { this->voltage_frontend_.pga_gain = value; }
  void set_current_sample_res(float value) { this->current_frontend_.sample_res = value; }
  void set_current_sample_ratio(float value) { this->current_frontend_.sample_ratio = value; }
  void set_current_pga_gain(uint8_t value) { this->current_frontend_.pga_gain = value; }

  // Calibration trim setters
  void set_voltage_reference(float ref) { this->voltage_reference_ = ref; }
  void set_current_reference(float ref) { this->current_reference_ = ref; }
  void set_power_reference(float ref) { this->power_reference_ = ref; }
  void set_energy_reference(float ref) { this->energy_reference_ = ref; }
  void set_cfdiv(uint16_t value) { this->cfdiv_ = value & 0x0FFF; }
  void set_preferences_key(const std::string &value) { this->preferences_key_ = value; }

  // Read retry behavior
  void set_max_read_attempts(uint8_t value) { this->max_read_attempts_ = value; }
  void set_immediate_read_attempts(uint8_t value) { this->immediate_read_attempts_ = value; }
  void set_retry_backoff_base_ms(uint16_t value) { this->retry_backoff_base_ms_ = value; }
  void set_retry_backoff_multiplier(uint8_t value) { this->retry_backoff_multiplier_ = value; }

  // Global sensor setters
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_frequency_sensor(sensor::Sensor *sensor) { this->frequency_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_total_power_sensor(sensor::Sensor *sensor) { this->total_power_sensor_ = sensor; }
  void set_total_energy_sensor(sensor::Sensor *sensor) { this->total_energy_sensor_ = sensor; }

  // Per-channel sensor setters
  void set_current_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->current_sensors_[channel] = sensor;
  }
  void set_power_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->power_sensors_[channel] = sensor;
  }
  void set_energy_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->energy_sensors_[channel] = sensor;
  }

  // Public actions
  void reset_energy();
  void calibrate_zero();

 protected:
  // UART communication
  bool read_register_(uint8_t reg, uint32_t &value);
  bool write_register_(uint8_t reg, uint32_t value);

  // Initialization
  bool chip_init_();
  bool apply_calibration_();
  void recalculate_scales_();
  void load_energy_state_();
  void save_energy_state_(bool force = false);

  // PGA helpers
  static uint8_t encode_pga_gain_(uint8_t gain);
  static int16_t gain_to_register_(float gain);

  // Value conversion
  float convert_voltage_(uint32_t raw);
  float convert_current_(uint32_t raw);
  float convert_power_(int32_t raw);
  float convert_energy_(uint32_t raw);
  float convert_frequency_(uint32_t raw);
  float convert_temperature_(uint32_t raw);

  // Frontend configuration
  FrontendConfig voltage_frontend_{};
  FrontendConfig current_frontend_{};
  float voltage_reference_{1.0f};
  float current_reference_{1.0f};
  float power_reference_{1.0f};
  float energy_reference_{1.0f};
  uint16_t cfdiv_{0x010};

  // Derived scales (computed from frontend config)
  float voltage_scale_{1.0f};
  float current_scale_{1.0f};
  float power_scale_{1.0f};

  // Global sensors
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *total_power_sensor_{nullptr};
  sensor::Sensor *total_energy_sensor_{nullptr};

  // Per-channel sensors
  sensor::Sensor *current_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *power_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *energy_sensors_[NUM_CHANNELS]{nullptr};

  // Energy accumulation (delta-based with 24-bit wraparound)
  uint32_t last_cf_cnt_[NUM_CHANNELS]{0};
  uint32_t last_cf_cnt_total_{0};
  bool last_cf_cnt_valid_[NUM_CHANNELS]{false};
  bool last_cf_cnt_total_valid_{false};
  float accumulated_energy_[NUM_CHANNELS]{0.0f};
  float accumulated_energy_total_{0.0f};
  bool energy_state_dirty_{false};
  uint32_t last_energy_save_ms_{0};
  ESPPreferenceObject energy_pref_;

  // Persistent RMSOS calibration (saved to NVS)
  int32_t saved_rmsos_[NUM_CHANNELS]{0};
  ESPPreferenceObject rmsos_pref_;
  std::string preferences_key_{"bl0906"};

  // State
  bool initialized_{false};

  // Read retry behavior
  uint8_t max_read_attempts_{BL0906_DEFAULT_MAX_READ_ATTEMPTS};
  uint8_t immediate_read_attempts_{BL0906_DEFAULT_IMMEDIATE_READ_ATTEMPTS};
  uint16_t retry_backoff_base_ms_{BL0906_DEFAULT_RETRY_BACKOFF_BASE_MS};
  uint8_t retry_backoff_multiplier_{BL0906_DEFAULT_RETRY_BACKOFF_MULTIPLIER};
};

template<typename... Ts>
class ResetEnergyAction : public Action<Ts...>, public Parented<BL0906Component> {
 public:
  void play(Ts... x) override { this->parent_->reset_energy(); }
};

template<typename... Ts>
class CalibrateZeroAction : public Action<Ts...>, public Parented<BL0906Component> {
 public:
  void play(Ts... x) override { this->parent_->calibrate_zero(); }
};

}  // namespace bl0906
}  // namespace esphome

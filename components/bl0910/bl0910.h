#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bl0910 {

// BL0910 Register Addresses
static const uint8_t REG_WAVE_1 = 0x01;
static const uint8_t REG_WAVE_2 = 0x02;
static const uint8_t REG_WAVE_3 = 0x03;
static const uint8_t REG_WAVE_4 = 0x04;
static const uint8_t REG_WAVE_5 = 0x05;
static const uint8_t REG_WAVE_6 = 0x06;
static const uint8_t REG_WAVE_7 = 0x07;
static const uint8_t REG_WAVE_8 = 0x08;
static const uint8_t REG_WAVE_9 = 0x09;
static const uint8_t REG_WAVE_10 = 0x0A;
static const uint8_t REG_WAVE_11 = 0x0B;  // Voltage waveform

static const uint8_t REG_RMS_1 = 0x0C;
static const uint8_t REG_RMS_2 = 0x0D;
static const uint8_t REG_RMS_3 = 0x0E;
static const uint8_t REG_RMS_4 = 0x0F;
static const uint8_t REG_RMS_5 = 0x10;
static const uint8_t REG_RMS_6 = 0x11;
static const uint8_t REG_RMS_7 = 0x12;
static const uint8_t REG_RMS_8 = 0x13;
static const uint8_t REG_RMS_9 = 0x14;
static const uint8_t REG_RMS_10 = 0x15;
static const uint8_t REG_RMS_11 = 0x16;  // Voltage RMS

static const uint8_t REG_FAST_RMS_1 = 0x17;
static const uint8_t REG_FAST_RMS_2 = 0x18;
static const uint8_t REG_FAST_RMS_3 = 0x19;
static const uint8_t REG_FAST_RMS_4 = 0x1A;
static const uint8_t REG_FAST_RMS_5 = 0x1B;
static const uint8_t REG_FAST_RMS_6 = 0x1C;
static const uint8_t REG_FAST_RMS_7 = 0x1D;
static const uint8_t REG_FAST_RMS_8 = 0x1E;
static const uint8_t REG_FAST_RMS_9 = 0x1F;
static const uint8_t REG_FAST_RMS_10 = 0x20;
static const uint8_t REG_FAST_RMS_11 = 0x21;

static const uint8_t REG_WATT_1 = 0x22;
static const uint8_t REG_WATT_2 = 0x23;
static const uint8_t REG_WATT_3 = 0x24;
static const uint8_t REG_WATT_4 = 0x25;
static const uint8_t REG_WATT_5 = 0x26;
static const uint8_t REG_WATT_6 = 0x27;
static const uint8_t REG_WATT_7 = 0x28;
static const uint8_t REG_WATT_8 = 0x29;
static const uint8_t REG_WATT_9 = 0x2A;
static const uint8_t REG_WATT_10 = 0x2B;
static const uint8_t REG_WATT_TOTAL = 0x2C;

static const uint8_t REG_VAR = 0x2D;       // Reactive power (selected channel)
static const uint8_t REG_VA = 0x2E;        // Apparent power (selected channel)

static const uint8_t REG_CF_CNT_1 = 0x2F;
static const uint8_t REG_CF_CNT_2 = 0x30;
static const uint8_t REG_CF_CNT_3 = 0x31;
static const uint8_t REG_CF_CNT_4 = 0x32;
static const uint8_t REG_CF_CNT_5 = 0x33;
static const uint8_t REG_CF_CNT_6 = 0x34;
static const uint8_t REG_CF_CNT_7 = 0x35;
static const uint8_t REG_CF_CNT_8 = 0x36;
static const uint8_t REG_CF_CNT_9 = 0x37;
static const uint8_t REG_CF_CNT_10 = 0x38;
static const uint8_t REG_CF_CNT_TOTAL = 0x39;
static const uint8_t REG_CFQ_CNT = 0x3A;   // Reactive energy counter
static const uint8_t REG_CFS_CNT = 0x3B;   // Apparent energy counter

static const uint8_t REG_ANGLE_1 = 0x3C;
static const uint8_t REG_ANGLE_2 = 0x3D;
static const uint8_t REG_ANGLE_3 = 0x3E;
static const uint8_t REG_ANGLE_4 = 0x3F;
static const uint8_t REG_ANGLE_5 = 0x40;
static const uint8_t REG_ANGLE_6 = 0x41;
static const uint8_t REG_ANGLE_7 = 0x42;
static const uint8_t REG_ANGLE_8 = 0x43;
static const uint8_t REG_ANGLE_9 = 0x44;
static const uint8_t REG_ANGLE_10 = 0x45;

static const uint8_t REG_PF = 0x4A;        // Power factor (selected channel)
static const uint8_t REG_PERIOD = 0x4E;    // Line period

static const uint8_t REG_TPS1 = 0x5E;      // Internal temperature
static const uint8_t REG_TPS2 = 0x5F;      // External temperature

static const uint8_t REG_GAIN1 = 0x60;     // PGA gain channels 1-5 + voltage
static const uint8_t REG_GAIN2 = 0x61;     // PGA gain channels 6-10
static const uint8_t REG_RMSGN_1 = 0x6C;
static const uint8_t REG_RMSOS_1 = 0x77;
static const uint8_t REG_WATTGN_1 = 0xB6;
static const uint8_t REG_WATTOS_1 = 0xC0;
static const uint8_t REG_CFDIV = 0xCE;

static const uint8_t REG_MODE1 = 0x96;     // Mode register 1
static const uint8_t REG_MODE = 0x98;      // Mode register

static const uint8_t REG_MASK1 = 0x9A;       // IRQ1 mask register
static const uint8_t REG_USR_WRPROT = 0x9E;  // Write protection
static const uint8_t REG_SOFT_RESET = 0x9F;  // Soft reset

static const uint8_t REG_STATUS1 = 0x54;     // Interrupt status flags

// SPI Commands
static const uint8_t SPI_READ_CMD = 0x82;
static const uint8_t SPI_WRITE_CMD = 0x81;

// Special values
static const uint32_t USR_WRPROT_UNLOCK = 0x5555;
static const uint32_t SOFT_RESET_VALUE = 0x5A5A5A;

// Number of channels
static const uint8_t NUM_CHANNELS = 10;

// Retry count for SPI operations
static const uint8_t SPI_RETRY_COUNT = 5;

enum LineFrequency : uint8_t {
  LINE_FREQUENCY_50HZ = 0,
  LINE_FREQUENCY_60HZ = 1,
};

enum MeasurementMode : uint8_t {
  MODE_1U10I = 0,
  MODE_5U5I = 1,
  MODE_3U6I = 2,
};

struct FrontendConfig {
  float load_res{0.0f};
  float sample_res{1.0f};
  float sample_ratio{1.0f};
  uint8_t pga_gain{1};
};

class BL0910Component : public PollingComponent,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Pin setters
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_irq_pin(GPIOPin *pin) { this->irq_pin_ = pin; }

  // Configuration setters
  void set_mode(MeasurementMode mode) { this->mode_ = mode; }
  void set_line_frequency(LineFrequency freq) { this->line_frequency_ = freq; }
  void set_voltage_reference(float ref) { this->voltage_reference_ = ref; }
  void set_current_reference(float ref) { this->current_reference_ = ref; }
  void set_power_reference(float ref) { this->power_reference_ = ref; }
  void set_energy_reference(float ref) { this->energy_reference_ = ref; }
  void set_voltage_load_res(float value);
  void set_voltage_sample_res(float value);
  void set_voltage_sample_ratio(float value);
  void set_voltage_pga_gain(uint8_t value);
  void set_current_sample_res(float value);
  void set_current_sample_ratio(float value);
  void set_current_pga_gain(uint8_t value);
  void set_cfdiv(uint16_t value) { this->cfdiv_ = value & 0x0FFF; }

  // Sensor setters - global sensors
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_frequency_sensor(sensor::Sensor *sensor) { this->frequency_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_total_power_sensor(sensor::Sensor *sensor) { this->total_power_sensor_ = sensor; }
  void set_total_energy_sensor(sensor::Sensor *sensor) { this->total_energy_sensor_ = sensor; }
  void set_channel_voltage_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->voltage_channel_sensors_[channel] = sensor;
  }

  // Sensor setters - per channel
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
  void set_power_factor_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->power_factor_sensors_[channel] = sensor;
  }

  // Public reset methods
  void hardware_reset();
  void reinitialize();
  bool write_irq_mask(uint32_t mask);

  // Cached measurement getters (populated every update cycle)
  bool is_initialized() const { return this->initialized_; }
  float get_voltage_rms() const { return this->cached_voltage_rms_; }
  float get_voltage_rms(uint8_t channel) const {
    return (channel < NUM_CHANNELS) ? this->cached_voltage_rms_by_channel_[channel] : NAN;
  }
  float get_current_rms(uint8_t channel) const {
    return (channel < NUM_CHANNELS) ? this->cached_current_rms_[channel] : NAN;
  }
  float get_active_power(uint8_t channel) const {
    return (channel < NUM_CHANNELS) ? this->cached_active_power_[channel] : NAN;
  }
  float get_total_active_power() const { return this->cached_total_active_power_; }
  float get_frequency() const { return this->cached_frequency_; }

 protected:
  // SPI communication methods
  bool read_register_(uint8_t reg, uint32_t &value);
  bool write_register_(uint8_t reg, uint32_t value);
  void reset_spi_();
  bool write_user_register_(uint8_t reg, uint32_t value);

  // Initialization methods
  void hardware_reset_();
  bool chip_init_();
  bool configure_mode_();
  bool apply_calibration_();
  void recalculate_scales_();

  // Mode helpers
  bool is_valid_measurement_channel_(uint8_t channel) const;
  int8_t get_current_input_index_(uint8_t channel) const;
  int8_t get_voltage_input_index_(uint8_t channel) const;
  bool is_current_input_(uint8_t input_index) const;
  bool is_voltage_input_(uint8_t input_index) const;
  int8_t get_primary_voltage_input_index_() const;
  static uint8_t encode_pga_gain_(uint8_t gain);
  static int16_t gain_to_register_(float gain);

  // Value conversion methods
  float convert_voltage_(uint32_t raw);
  float convert_current_(uint32_t raw);
  float convert_power_(int32_t raw);
  float convert_energy_(uint32_t raw);
  float convert_frequency_(uint32_t raw);
  float convert_temperature_(uint32_t raw);
  float convert_power_factor_(int32_t raw);

  // Pins
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *irq_pin_{nullptr};

  // Configuration
  MeasurementMode mode_{MODE_1U10I};
  LineFrequency line_frequency_{LINE_FREQUENCY_50HZ};
  float voltage_reference_{1.0f};
  float current_reference_{1.0f};
  float power_reference_{1.0f};
  float energy_reference_{1.0f};
  FrontendConfig voltage_frontend_{2000000.0f, 510.0f, 1.0f, 1};
  FrontendConfig current_frontend_{0.0f, 0.001f, 1.0f, 1};
  uint16_t cfdiv_{0x010};
  float voltage_scale_{1.0f};
  float current_scale_{1.0f};
  float power_scale_{1.0f};

  // Global sensors
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *total_power_sensor_{nullptr};
  sensor::Sensor *total_energy_sensor_{nullptr};

  // Per-channel sensors (10 channels)
  sensor::Sensor *current_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *voltage_channel_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *power_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *energy_sensors_[NUM_CHANNELS]{nullptr};
  sensor::Sensor *power_factor_sensors_[NUM_CHANNELS]{nullptr};

  // Energy accumulation (for kWh tracking)
  uint32_t last_cf_cnt_[NUM_CHANNELS]{0};
  uint32_t last_cf_cnt_total_{0};
  float accumulated_energy_[NUM_CHANNELS]{0.0f};
  float accumulated_energy_total_{0.0f};

  // Cached measurements (always populated in update, regardless of sensor config)
  float cached_voltage_rms_{NAN};
  float cached_voltage_rms_by_channel_[NUM_CHANNELS]{};
  float cached_current_rms_[NUM_CHANNELS]{};
  float cached_active_power_[NUM_CHANNELS]{};
  float cached_total_active_power_{NAN};
  float cached_frequency_{NAN};

  // State
  bool initialized_{false};
  
  // Error tracking for diagnostics
  const char *error_reason_{nullptr};
  uint8_t last_failed_register_{0};
  uint8_t last_rx_data_[4]{0};
};

template<typename... Ts>
class ResetAction : public Action<Ts...>, public Parented<BL0910Component> {
 public:
  void play(Ts... x) override { this->parent_->hardware_reset(); }
};

}  // namespace bl0910
}  // namespace esphome

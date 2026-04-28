#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../bl0910/bl0910.h"

namespace esphome {
namespace bl0910_3phase {

static const uint8_t NUM_PHASES = 3;

// MASK1 value to unmask only bit 12 (voltage ZX on channel 11)
static const uint32_t MASK1_VOLTAGE_ZX_ONLY = 0xFFEFFF;

enum LineFrequency : uint8_t {
  LINE_FREQUENCY_50HZ = 0,
  LINE_FREQUENCY_60HZ = 1,
};

class BL09103PhaseComponent : public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Phase chip setters
  void set_phase_a(bl0910::BL0910Component *chip) { this->phases_[0] = chip; }
  void set_phase_b(bl0910::BL0910Component *chip) { this->phases_[1] = chip; }
  void set_phase_c(bl0910::BL0910Component *chip) { this->phases_[2] = chip; }

  // Reset pin setters
  void set_shared_reset_pin(GPIOPin *pin) {
    this->reset_pins_[0] = pin;
    this->num_reset_pins_ = 1;
  }
  void set_reset_pin(uint8_t index, GPIOPin *pin) {
    if (index < NUM_PHASES) {
      this->reset_pins_[index] = pin;
      this->num_reset_pins_ = NUM_PHASES;
    }
  }

  // IRQ1 pin setters (primary -- configurable via MASK1)
  void set_irq1_a_pin(InternalGPIOPin *pin) { this->irq1_pins_[0] = pin; }
  void set_irq1_b_pin(InternalGPIOPin *pin) { this->irq1_pins_[1] = pin; }
  void set_irq1_c_pin(InternalGPIOPin *pin) { this->irq1_pins_[2] = pin; }

  // IRQ2 pin setters (alternative -- hardwired voltage ZX)
  void set_irq2_a_pin(InternalGPIOPin *pin) { this->irq2_pins_[0] = pin; }
  void set_irq2_b_pin(InternalGPIOPin *pin) { this->irq2_pins_[1] = pin; }
  void set_irq2_c_pin(InternalGPIOPin *pin) { this->irq2_pins_[2] = pin; }

  // Configuration
  void set_line_frequency(LineFrequency freq) { this->line_frequency_ = freq; }

  // Sensor setters
  void set_total_active_power_sensor(sensor::Sensor *s) { this->total_active_power_sensor_ = s; }
  void set_total_reactive_power_sensor(sensor::Sensor *s) { this->total_reactive_power_sensor_ = s; }
  void set_total_apparent_power_sensor(sensor::Sensor *s) { this->total_apparent_power_sensor_ = s; }
  void set_system_power_factor_sensor(sensor::Sensor *s) { this->system_power_factor_sensor_ = s; }
  void set_frequency_sensor(sensor::Sensor *s) { this->frequency_sensor_ = s; }
  void set_phase_angle_ab_sensor(sensor::Sensor *s) { this->phase_angle_sensors_[0] = s; }
  void set_phase_angle_bc_sensor(sensor::Sensor *s) { this->phase_angle_sensors_[1] = s; }
  void set_phase_angle_ca_sensor(sensor::Sensor *s) { this->phase_angle_sensors_[2] = s; }
  void set_voltage_unbalance_sensor(sensor::Sensor *s) { this->voltage_unbalance_sensor_ = s; }
  void set_line_voltage_ab_sensor(sensor::Sensor *s) { this->line_voltage_sensors_[0] = s; }
  void set_line_voltage_bc_sensor(sensor::Sensor *s) { this->line_voltage_sensors_[1] = s; }
  void set_line_voltage_ca_sensor(sensor::Sensor *s) { this->line_voltage_sensors_[2] = s; }
  void set_phase_sequence_sensor(text_sensor::TextSensor *s) { this->phase_sequence_sensor_ = s; }

  // ISR-accessible timestamp storage
  volatile uint32_t zx_us_[NUM_PHASES]{0};

 protected:
  void perform_hardware_reset_();
  void configure_irq1_masks_();
  void attach_irq_pins_();
  void compute_phase_angles_();
  void compute_power_totals_();
  void compute_line_voltages_();
  void compute_unbalance_();
  void publish_sensors_();

  static float normalize_angle_(float angle);

  // Phase chip references
  bl0910::BL0910Component *phases_[NUM_PHASES]{nullptr};

  // Reset pins (1 shared or 3 separate)
  GPIOPin *reset_pins_[NUM_PHASES]{nullptr};
  uint8_t num_reset_pins_{0};

  // IRQ pins
  InternalGPIOPin *irq1_pins_[NUM_PHASES]{nullptr};
  InternalGPIOPin *irq2_pins_[NUM_PHASES]{nullptr};

  // Configuration
  LineFrequency line_frequency_{LINE_FREQUENCY_50HZ};

  // State
  bool irq_masks_configured_{false};

  // Computed 3-phase values
  float phase_angles_[NUM_PHASES]{NAN, NAN, NAN};   // AB, BC, CA in degrees
  float total_active_power_{NAN};
  float total_reactive_power_{NAN};
  float total_apparent_power_{NAN};
  float system_power_factor_{NAN};
  float frequency_{NAN};
  float line_voltages_[NUM_PHASES]{NAN, NAN, NAN};   // AB, BC, CA
  float voltage_unbalance_{NAN};

  // EMA smoothing for phase angles
  static constexpr float ANGLE_EMA_ALPHA = 0.2f;
  float smoothed_angles_[NUM_PHASES]{NAN, NAN, NAN};

  // Sensors
  sensor::Sensor *total_active_power_sensor_{nullptr};
  sensor::Sensor *total_reactive_power_sensor_{nullptr};
  sensor::Sensor *total_apparent_power_sensor_{nullptr};
  sensor::Sensor *system_power_factor_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *phase_angle_sensors_[NUM_PHASES]{nullptr};
  sensor::Sensor *voltage_unbalance_sensor_{nullptr};
  sensor::Sensor *line_voltage_sensors_[NUM_PHASES]{nullptr};
  text_sensor::TextSensor *phase_sequence_sensor_{nullptr};
};

}  // namespace bl0910_3phase
}  // namespace esphome

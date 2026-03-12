#include "bl0910_3phase.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0910_3phase {

static const char *const TAG = "bl0910_3phase";

static void IRAM_ATTR zx_isr_a(BL09103PhaseComponent *comp) {
  comp->zx_us_[0] = micros();
}
static void IRAM_ATTR zx_isr_b(BL09103PhaseComponent *comp) {
  comp->zx_us_[1] = micros();
}
static void IRAM_ATTR zx_isr_c(BL09103PhaseComponent *comp) {
  comp->zx_us_[2] = micros();
}

void BL09103PhaseComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BL0910 3-Phase...");
  this->perform_hardware_reset_();
}

void BL09103PhaseComponent::loop() {
  if (this->irq_masks_configured_)
    return;

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->phases_[i] == nullptr || !this->phases_[i]->is_initialized())
      return;
  }

  this->configure_irq1_masks_();
  this->attach_irq_pins_();
  this->irq_masks_configured_ = true;
  ESP_LOGI(TAG, "3-Phase IRQ configuration complete");
}

void BL09103PhaseComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "BL0910 3-Phase:");

  if (this->num_reset_pins_ == 1) {
    LOG_PIN("  Shared Reset Pin: ", this->reset_pins_[0]);
  } else if (this->num_reset_pins_ == NUM_PHASES) {
    LOG_PIN("  Reset Pin A: ", this->reset_pins_[0]);
    LOG_PIN("  Reset Pin B: ", this->reset_pins_[1]);
    LOG_PIN("  Reset Pin C: ", this->reset_pins_[2]);
  }

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    char phase = 'A' + i;
    if (this->irq1_pins_[i] != nullptr)
      ESP_LOGCONFIG(TAG, "  IRQ1_%c Pin: GPIO%u", phase, this->irq1_pins_[i]->get_pin());
    if (this->irq2_pins_[i] != nullptr)
      ESP_LOGCONFIG(TAG, "  IRQ2_%c Pin: GPIO%u", phase, this->irq2_pins_[i]->get_pin());
  }

  ESP_LOGCONFIG(TAG, "  Line Frequency: %s",
                this->line_frequency_ == LINE_FREQUENCY_50HZ ? "50Hz" : "60Hz");
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Total Active Power", this->total_active_power_sensor_);
  LOG_SENSOR("  ", "Total Reactive Power", this->total_reactive_power_sensor_);
  LOG_SENSOR("  ", "Total Apparent Power", this->total_apparent_power_sensor_);
  LOG_SENSOR("  ", "System Power Factor", this->system_power_factor_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Phase Angle A-B", this->phase_angle_sensors_[0]);
  LOG_SENSOR("  ", "Phase Angle B-C", this->phase_angle_sensors_[1]);
  LOG_SENSOR("  ", "Phase Angle C-A", this->phase_angle_sensors_[2]);
  LOG_SENSOR("  ", "Voltage Unbalance", this->voltage_unbalance_sensor_);
  LOG_SENSOR("  ", "Line Voltage A-B", this->line_voltage_sensors_[0]);
  LOG_SENSOR("  ", "Line Voltage B-C", this->line_voltage_sensors_[1]);
  LOG_SENSOR("  ", "Line Voltage C-A", this->line_voltage_sensors_[2]);
}

void BL09103PhaseComponent::update() {
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->phases_[i] == nullptr || !this->phases_[i]->is_initialized()) {
      ESP_LOGW(TAG, "Phase %c chip not ready, skipping update", 'A' + i);
      return;
    }
  }

  this->compute_phase_angles_();
  this->compute_power_totals_();
  this->compute_line_voltages_();
  this->compute_unbalance_();
  this->publish_sensors_();
}

// --- Hardware Reset ---

void BL09103PhaseComponent::perform_hardware_reset_() {
  if (this->num_reset_pins_ == 0)
    return;

  ESP_LOGD(TAG, "Performing hardware reset (%s)",
           this->num_reset_pins_ == 1 ? "shared pin" : "per-chip pins");

  if (this->num_reset_pins_ == 1) {
    this->reset_pins_[0]->setup();
    this->reset_pins_[0]->digital_write(false);
  } else {
    for (uint8_t i = 0; i < NUM_PHASES; i++) {
      if (this->reset_pins_[i] != nullptr) {
        this->reset_pins_[i]->setup();
        this->reset_pins_[i]->digital_write(false);
      }
    }
  }

  delay(20);

  if (this->num_reset_pins_ == 1) {
    this->reset_pins_[0]->digital_write(true);
  } else {
    for (uint8_t i = 0; i < NUM_PHASES; i++) {
      if (this->reset_pins_[i] != nullptr)
        this->reset_pins_[i]->digital_write(true);
    }
  }

  delay(100);
  ESP_LOGD(TAG, "Hardware reset complete");
}

// --- IRQ Configuration ---

void BL09103PhaseComponent::configure_irq1_masks_() {
  bool has_irq1 = false;
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->irq1_pins_[i] != nullptr)
      has_irq1 = true;
  }

  if (!has_irq1)
    return;

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->irq1_pins_[i] != nullptr && this->phases_[i] != nullptr) {
      if (this->phases_[i]->write_irq_mask(MASK1_VOLTAGE_ZX_ONLY)) {
        ESP_LOGD(TAG, "Configured MASK1 on phase %c for voltage ZX", 'A' + i);
      } else {
        ESP_LOGE(TAG, "Failed to configure MASK1 on phase %c", 'A' + i);
      }
    }
  }
}

void BL09103PhaseComponent::attach_irq_pins_() {
  using isr_func_t = void (*)(BL09103PhaseComponent *);
  static const isr_func_t ISR_FUNCS[NUM_PHASES] = {zx_isr_a, zx_isr_b, zx_isr_c};

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    InternalGPIOPin *pin = this->irq1_pins_[i];
    const char *src = "IRQ1";
    if (pin == nullptr) {
      pin = this->irq2_pins_[i];
      src = "IRQ2";
    }
    if (pin == nullptr)
      continue;

    pin->setup();
    pin->attach_interrupt(ISR_FUNCS[i], this, gpio::INTERRUPT_FALLING_EDGE);
    ESP_LOGD(TAG, "Attached %s ISR for phase %c", src, 'A' + i);
  }
}

// --- Phase Angle Computation ---

float BL09103PhaseComponent::normalize_angle_(float angle) {
  while (angle > 180.0f)
    angle -= 360.0f;
  while (angle < -180.0f)
    angle += 360.0f;
  return angle;
}

void BL09103PhaseComponent::compute_phase_angles_() {
  uint32_t ts[NUM_PHASES];
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    ts[i] = this->zx_us_[i];
    if (ts[i] == 0)
      return;
  }

  float freq = 0;
  uint8_t freq_count = 0;
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    float f = this->phases_[i]->get_frequency();
    if (!std::isnan(f) && f > 0) {
      freq += f;
      freq_count++;
    }
  }
  if (freq_count == 0)
    return;
  freq /= freq_count;
  this->frequency_ = freq;

  float t_period_us = 1000000.0f / freq;

  // Raw inter-phase angles: AB, BC, CA
  float raw_angles[NUM_PHASES];
  raw_angles[0] = static_cast<float>(static_cast<int32_t>(ts[1] - ts[0])) / t_period_us * 360.0f;
  raw_angles[1] = static_cast<float>(static_cast<int32_t>(ts[2] - ts[1])) / t_period_us * 360.0f;
  raw_angles[2] = static_cast<float>(static_cast<int32_t>(ts[0] - ts[2])) / t_period_us * 360.0f;

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    float normalized = normalize_angle_(raw_angles[i]);

    if (std::isnan(this->smoothed_angles_[i])) {
      this->smoothed_angles_[i] = normalized;
    } else {
      float diff = normalized - this->smoothed_angles_[i];
      if (diff > 180.0f)
        diff -= 360.0f;
      if (diff < -180.0f)
        diff += 360.0f;
      this->smoothed_angles_[i] += ANGLE_EMA_ALPHA * diff;
      this->smoothed_angles_[i] = normalize_angle_(this->smoothed_angles_[i]);
    }

    this->phase_angles_[i] = this->smoothed_angles_[i];
  }
}

// --- Power Totals ---

void BL09103PhaseComponent::compute_power_totals_() {
  float p_total = 0;
  float q_total = 0;

  for (uint8_t ph = 0; ph < NUM_PHASES; ph++) {
    float p_phase = this->phases_[ph]->get_total_active_power();
    float v_rms = this->phases_[ph]->get_voltage_rms();

    if (std::isnan(p_phase) || std::isnan(v_rms))
      return;

    p_total += p_phase;

    float s_phase = 0;
    for (uint8_t ch = 0; ch < 10; ch++) {
      float i_rms = this->phases_[ph]->get_current_rms(ch);
      if (!std::isnan(i_rms))
        s_phase += v_rms * i_rms;
    }

    // Q_phase = sqrt(S_phase^2 - P_phase^2)
    float s2 = s_phase * s_phase;
    float p2 = p_phase * p_phase;
    q_total += (s2 >= p2) ? std::sqrt(s2 - p2) : 0.0f;
  }

  this->total_active_power_ = p_total;
  this->total_reactive_power_ = q_total;
  this->total_apparent_power_ = std::sqrt(p_total * p_total + q_total * q_total);

  if (this->total_apparent_power_ > 0.001f) {
    this->system_power_factor_ = p_total / this->total_apparent_power_;
  } else {
    this->system_power_factor_ = 1.0f;
  }
}

// --- Line Voltages ---

void BL09103PhaseComponent::compute_line_voltages_() {
  float v[NUM_PHASES];
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    v[i] = this->phases_[i]->get_voltage_rms();
    if (std::isnan(v[i]))
      return;
  }

  // V_LL = sqrt(V_a^2 + V_b^2 - 2*V_a*V_b*cos(theta))
  // AB: phases 0,1 angle[0]; BC: phases 1,2 angle[1]; CA: phases 2,0 angle[2]
  static const uint8_t PAIRS[][2] = {{0, 1}, {1, 2}, {2, 0}};

  for (uint8_t p = 0; p < NUM_PHASES; p++) {
    float va = v[PAIRS[p][0]];
    float vb = v[PAIRS[p][1]];
    float theta_deg = std::isnan(this->phase_angles_[p]) ? 120.0f : this->phase_angles_[p];
    float theta_rad = theta_deg * static_cast<float>(M_PI) / 180.0f;
    float v_ll_sq = va * va + vb * vb - 2.0f * va * vb * std::cos(theta_rad);
    this->line_voltages_[p] = (v_ll_sq > 0) ? std::sqrt(v_ll_sq) : 0.0f;
  }
}

// --- Unbalance ---

void BL09103PhaseComponent::compute_unbalance_() {
  float v[NUM_PHASES];
  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    v[i] = this->phases_[i]->get_voltage_rms();
    if (std::isnan(v[i]))
      return;
  }

  float v_max = v[0], v_min = v[0];
  for (uint8_t i = 1; i < NUM_PHASES; i++) {
    if (v[i] > v_max)
      v_max = v[i];
    if (v[i] < v_min)
      v_min = v[i];
  }

  float v_avg = (v[0] + v[1] + v[2]) / 3.0f;
  if (v_avg > 0.1f) {
    this->voltage_unbalance_ = (v_max - v_min) / v_avg * 100.0f;
  } else {
    this->voltage_unbalance_ = 0.0f;
  }
}

// --- Publish ---

void BL09103PhaseComponent::publish_sensors_() {
  if (this->total_active_power_sensor_ != nullptr && !std::isnan(this->total_active_power_))
    this->total_active_power_sensor_->publish_state(this->total_active_power_);

  if (this->total_reactive_power_sensor_ != nullptr && !std::isnan(this->total_reactive_power_))
    this->total_reactive_power_sensor_->publish_state(this->total_reactive_power_);

  if (this->total_apparent_power_sensor_ != nullptr && !std::isnan(this->total_apparent_power_))
    this->total_apparent_power_sensor_->publish_state(this->total_apparent_power_);

  if (this->system_power_factor_sensor_ != nullptr && !std::isnan(this->system_power_factor_))
    this->system_power_factor_sensor_->publish_state(this->system_power_factor_);

  if (this->frequency_sensor_ != nullptr && !std::isnan(this->frequency_))
    this->frequency_sensor_->publish_state(this->frequency_);

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->phase_angle_sensors_[i] != nullptr && !std::isnan(this->phase_angles_[i]))
      this->phase_angle_sensors_[i]->publish_state(this->phase_angles_[i]);
  }

  if (this->voltage_unbalance_sensor_ != nullptr && !std::isnan(this->voltage_unbalance_))
    this->voltage_unbalance_sensor_->publish_state(this->voltage_unbalance_);

  for (uint8_t i = 0; i < NUM_PHASES; i++) {
    if (this->line_voltage_sensors_[i] != nullptr && !std::isnan(this->line_voltages_[i]))
      this->line_voltage_sensors_[i]->publish_state(this->line_voltages_[i]);
  }

  if (this->phase_sequence_sensor_ != nullptr && !std::isnan(this->phase_angles_[0])) {
    const char *seq;
    if (this->phase_angles_[0] > 30.0f && this->phase_angles_[0] < 180.0f) {
      seq = "ABC";
    } else if (this->phase_angles_[0] < -30.0f && this->phase_angles_[0] > -180.0f) {
      seq = "ACB";
    } else {
      seq = "Unknown";
    }
    this->phase_sequence_sensor_->publish_state(seq);
  }
}

}  // namespace bl0910_3phase
}  // namespace esphome

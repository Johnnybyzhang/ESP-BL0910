#include "bl0906.h"
#include "constants.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace bl0906 {

static const char *const TAG = "bl0906";
static const uint32_t ENERGY_PERSIST_INTERVAL_MS = 60000;
static const float ZERO_CALIBRATION_MAX_CURRENT_A = 0.05f;

struct BL0906EnergyState {
  float channel[NUM_CHANNELS];
  float total;
};

static uint8_t compute_checksum_(uint8_t address, uint8_t l, uint8_t m, uint8_t h) {
  return (address + l + m + h) ^ 0xFF;
}

void BL0906Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BL0906...");
  this->recalculate_scales_();

  const uint32_t rmsos_hash = fnv1_hash(this->preferences_key_ + "_rmsos");
  this->rmsos_pref_ = global_preferences->make_preference<int32_t[NUM_CHANNELS]>(rmsos_hash);
  if (this->rmsos_pref_.load(&this->saved_rmsos_)) {
    ESP_LOGI(TAG, "Loaded RMSOS calibration from flash");
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      if (this->saved_rmsos_[i] != 0)
        ESP_LOGD(TAG, "  Ch%u RMSOS = %d", i + 1, this->saved_rmsos_[i]);
      }
  }

  const uint32_t energy_hash = fnv1_hash(this->preferences_key_ + "_energy");
  this->energy_pref_ = global_preferences->make_preference<BL0906EnergyState>(energy_hash);
  this->load_energy_state_();

  if (!this->chip_init_()) {
    ESP_LOGE(TAG, "BL0906 initialization failed!");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "BL0906 initialized successfully");
}

void BL0906Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BL0906:");
  ESP_LOGCONFIG(TAG, "  Voltage Frontend: load_res=%.1f ohm, sample_res=%.1f ohm, sample_ratio=%.6f, pga=%ux",
                this->voltage_frontend_.load_res, this->voltage_frontend_.sample_res,
                this->voltage_frontend_.sample_ratio, this->voltage_frontend_.pga_gain);
  ESP_LOGCONFIG(TAG, "  Current Frontend: sample_res=%.6f ohm, sample_ratio=%.6f, pga=%ux",
                this->current_frontend_.sample_res, this->current_frontend_.sample_ratio,
                this->current_frontend_.pga_gain);
  ESP_LOGCONFIG(TAG, "  Derived Voltage Scale: %.6f V_RMS FS", this->voltage_scale_);
  ESP_LOGCONFIG(TAG, "  Derived Current Scale: %.6f A_RMS FS", this->current_scale_);
  ESP_LOGCONFIG(TAG, "  Derived Power Scale: %.6f W FS", this->power_scale_);
  ESP_LOGCONFIG(TAG, "  Voltage Trim (RMSGN): %.6f", this->voltage_reference_);
  ESP_LOGCONFIG(TAG, "  Current Trim (RMSGN): %.6f", this->current_reference_);
  ESP_LOGCONFIG(TAG, "  Power Trim (WATTGN): %.6f", this->power_reference_);
  ESP_LOGCONFIG(TAG, "  Energy Reference: %.9f kWh/pulse", this->energy_reference_);
  ESP_LOGCONFIG(TAG, "  CFDIV: 0x%03X", this->cfdiv_);
  ESP_LOGCONFIG(TAG, "  Read Retry: max_attempts=%u, immediate_attempts=%u, backoff_base=%ums, backoff_multiplier=%u",
                this->max_read_attempts_, this->immediate_read_attempts_, this->retry_backoff_base_ms_,
                this->retry_backoff_multiplier_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Total Energy", this->total_energy_sensor_);

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->current_sensors_[i] == nullptr && this->power_sensors_[i] == nullptr &&
        this->energy_sensors_[i] == nullptr)
      continue;
    ESP_LOGCONFIG(TAG, "  Channel %u:", i + 1);
    LOG_SENSOR("    ", "Current", this->current_sensors_[i]);
    LOG_SENSOR("    ", "Power", this->power_sensors_[i]);
    LOG_SENSOR("    ", "Energy", this->energy_sensors_[i]);
  }
}

void BL0906Component::update() {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Not initialized, skipping update");
    return;
  }

  uint32_t raw;

  // Voltage
  if (this->read_register_(REG_V_RMS, raw)) {
    ESP_LOGV(TAG, "V_RMS raw=0x%06X (%u)", raw, raw);
    float voltage = this->convert_voltage_(raw);
    if (this->voltage_sensor_ != nullptr)
      this->voltage_sensor_->publish_state(voltage);
  }

  // Per-channel current
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->current_sensors_[i] != nullptr && this->read_register_(REG_I_RMS[i], raw)) {
      ESP_LOGV(TAG, "I_RMS[%u] raw=0x%06X (%u)", i + 1, raw, raw);
      this->current_sensors_[i]->publish_state(this->convert_current_(raw));
    }
  }

  // Per-channel active power (signed 24-bit)
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->power_sensors_[i] != nullptr && this->read_register_(REG_WATT[i], raw)) {
      int32_t signed_raw = static_cast<int32_t>(raw);
      if (signed_raw & 0x800000)
        signed_raw |= static_cast<int32_t>(0xFF000000);
      this->power_sensors_[i]->publish_state(this->convert_power_(signed_raw));
    }
  }

  // Total active power (signed 24-bit)
  if (this->total_power_sensor_ != nullptr && this->read_register_(REG_WATT_TOTAL, raw)) {
    int32_t signed_raw = static_cast<int32_t>(raw);
    if (signed_raw & 0x800000)
      signed_raw |= static_cast<int32_t>(0xFF000000);
    this->total_power_sensor_->publish_state(this->convert_power_(signed_raw));
  }

  // Per-channel energy (delta accumulation with 24-bit wraparound)
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->energy_sensors_[i] == nullptr)
      continue;

    if (this->read_register_(REG_CF_CNT[i], raw)) {
      uint32_t delta;
      if (!this->last_cf_cnt_valid_[i]) {
        delta = 0;
        this->last_cf_cnt_valid_[i] = true;
      } else if (raw >= this->last_cf_cnt_[i]) {
        delta = raw - this->last_cf_cnt_[i];
      } else {
        delta = (0xFFFFFF - this->last_cf_cnt_[i]) + raw + 1;
      }
      this->last_cf_cnt_[i] = raw;
      if (delta > 0) {
        this->accumulated_energy_[i] += this->convert_energy_(delta);
        this->energy_state_dirty_ = true;
      }
      this->energy_sensors_[i]->publish_state(this->accumulated_energy_[i]);
    }
  }

  // Total energy
  if (this->total_energy_sensor_ != nullptr && this->read_register_(REG_CF_CNT_TOTAL, raw)) {
    uint32_t delta;
    if (!this->last_cf_cnt_total_valid_) {
      delta = 0;
      this->last_cf_cnt_total_valid_ = true;
    } else if (raw >= this->last_cf_cnt_total_) {
      delta = raw - this->last_cf_cnt_total_;
    } else {
      delta = (0xFFFFFF - this->last_cf_cnt_total_) + raw + 1;
    }
    this->last_cf_cnt_total_ = raw;
    if (delta > 0) {
      this->accumulated_energy_total_ += this->convert_energy_(delta);
      this->energy_state_dirty_ = true;
    }
    this->total_energy_sensor_->publish_state(this->accumulated_energy_total_);
  }

  this->save_energy_state_();

  // Frequency
  if (this->read_register_(REG_PERIOD, raw)) {
    ESP_LOGV(TAG, "PERIOD raw=0x%06X (%u)", raw, raw);
    raw &= 0x0FFFFF;
    if (raw > 0) {
      float freq = this->convert_frequency_(raw);
      if (this->frequency_sensor_ != nullptr)
        this->frequency_sensor_->publish_state(freq);
    }
  }

  // Temperature
  if (this->read_register_(REG_TPS, raw)) {
    ESP_LOGV(TAG, "TPS raw=0x%06X (%u)", raw, raw);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(this->convert_temperature_(raw));
  }
}

// --- UART Communication ---

bool BL0906Component::read_register_(uint8_t reg, uint32_t &value) {
  const uint8_t attempts = this->max_read_attempts_ == 0 ? 1 : this->max_read_attempts_;
  const uint8_t immediate_attempts = this->immediate_read_attempts_ > attempts ? attempts : this->immediate_read_attempts_;

  for (uint8_t attempt = 1; attempt <= attempts; attempt++) {
    if (attempt > 1 && attempt > immediate_attempts && this->retry_backoff_base_ms_ > 0) {
      uint32_t delay_ms = this->retry_backoff_base_ms_;
      uint8_t exponent = attempt - immediate_attempts - 1;
      for (uint8_t i = 0; i < exponent; i++) {
        delay_ms *= this->retry_backoff_multiplier_;
      }
      delay(delay_ms);
    }

    // Drain stale RX
    while (this->available())
      this->read();

    // Send read command (t1 inter-byte < 20ms satisfied by write_array)
    const uint8_t cmd[2] = {UART_READ_CMD, reg};
    this->write_array(cmd, sizeof(cmd));
    this->flush();

    // Wait t3 (min 110us) for BL0906 to begin responding
    delayMicroseconds(BL0906_T3_WAIT_US);

    // Capture incoming bytes (possible echo + response) with timeouts
    uint8_t captured[BL0906_CAPTURE_MAX];
    size_t count = 0;
    uint32_t last_byte_us = micros();
    bool start_timeout = false;
    bool interbyte_timeout = false;

    while (count < BL0906_CAPTURE_MAX) {
      if (this->available()) {
        int b = this->read();
        if (b >= 0) {
          captured[count++] = static_cast<uint8_t>(b);
          last_byte_us = micros();
        }
      } else {
        uint32_t timeout = (count == 0) ? BL0906_RX_START_TIMEOUT_US : BL0906_RX_INTERBYTE_TIMEOUT_US;
        if (micros() - last_byte_us > timeout) {
          if (count == 0) {
            start_timeout = true;
          } else {
            interbyte_timeout = true;
          }
          break;
        }
      }
    }

    if (count < BL0906_FRAME_SIZE) {
      if (start_timeout) {
        ESP_LOGW(TAG, "Read failed reg 0x%02X attempt %u/%u: start timeout (%zu bytes)", reg, attempt, attempts, count);
      } else if (interbyte_timeout) {
        ESP_LOGW(TAG, "Read failed reg 0x%02X attempt %u/%u: inter-byte timeout (%zu bytes)", reg, attempt, attempts,
                 count);
      } else {
        ESP_LOGW(TAG, "Read failed reg 0x%02X attempt %u/%u: short frame (%zu bytes)", reg, attempt, attempts, count);
      }
      continue;
    }

    // Sliding-window checksum search for valid 4-byte frame {l, m, h, checksum}
    for (size_t i = 0; i <= count - BL0906_FRAME_SIZE; i++) {
      uint8_t l = captured[i];
      uint8_t m = captured[i + 1];
      uint8_t h = captured[i + 2];
      uint8_t cs = captured[i + 3];

      if (compute_checksum_(reg, l, m, h) == cs) {
        value = (static_cast<uint32_t>(h) << 16) | (static_cast<uint32_t>(m) << 8) | static_cast<uint32_t>(l);
        return true;
      }
    }

    ESP_LOGW(TAG, "Read failed reg 0x%02X attempt %u/%u: checksum mismatch (%zu bytes captured)", reg, attempt,
             attempts, count);
  }

  ESP_LOGW(TAG, "Read failed reg 0x%02X after %u attempts", reg, attempts);
  return false;
}

bool BL0906Component::write_register_(uint8_t reg, uint32_t value) {
  uint8_t l = value & 0xFF;
  uint8_t m = (value >> 8) & 0xFF;
  uint8_t h = (value >> 16) & 0xFF;
  uint8_t cs = compute_checksum_(reg, l, m, h);

  // Drain stale RX
  while (this->available())
    this->read();

  const uint8_t frame[6] = {UART_WRITE_CMD, reg, l, m, h, cs};
  this->write_array(frame, sizeof(frame));
  this->flush();

  delayMicroseconds(BL0906_T3_WAIT_US);
  return true;
}

// --- Initialization ---

bool BL0906Component::chip_init_() {
  // Drain any stale data
  while (this->available())
    this->read();

  // Verify communication by reading temperature sensor
  uint32_t raw;
  if (!this->read_register_(REG_TPS, raw)) {
    ESP_LOGE(TAG, "Communication check failed (TPS read)");
    return false;
  }
  ESP_LOGD(TAG, "TPS = 0x%06X -- comms OK", raw);

  if (!this->apply_calibration_())
    return false;

  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    this->last_cf_cnt_[i] = 0;
  this->last_cf_cnt_total_ = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    this->last_cf_cnt_valid_[i] = false;
  this->last_cf_cnt_total_valid_ = false;

  this->initialized_ = true;
  return true;
}

void BL0906Component::load_energy_state_() {
  BL0906EnergyState state{};
  if (!this->energy_pref_.load(&state))
    return;

  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    this->accumulated_energy_[i] = state.channel[i];
  this->accumulated_energy_total_ = state.total;
  this->last_energy_save_ms_ = millis();
  ESP_LOGI(TAG, "Loaded persisted energy totals from flash");
}

void BL0906Component::save_energy_state_(bool force) {
  if (!this->energy_state_dirty_)
    return;

  const uint32_t now = millis();
  if (!force && now - this->last_energy_save_ms_ < ENERGY_PERSIST_INTERVAL_MS)
    return;

  BL0906EnergyState state{};
  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    state.channel[i] = this->accumulated_energy_[i];
  state.total = this->accumulated_energy_total_;

  if (this->energy_pref_.save(&state)) {
    this->energy_state_dirty_ = false;
    this->last_energy_save_ms_ = now;
  }
}

bool BL0906Component::apply_calibration_() {
  // Unlock write protection
  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;

  // PGA gain configuration
  // GAIN1: [3:0]=Voltage, [11:8]=Ch1, [15:12]=Ch2, [19:16]=Ch3, [23:20]=Ch4
  uint32_t gain1 = encode_pga_gain_(this->voltage_frontend_.pga_gain);
  for (uint8_t i = 0; i < 4; i++) {
    gain1 |= static_cast<uint32_t>(encode_pga_gain_(this->current_frontend_.pga_gain)) << ((i + 2) * 4);
  }
  // GAIN2: [11:8]=Ch5, [15:12]=Ch6
  uint32_t gain2 = 0;
  for (uint8_t i = 0; i < 2; i++) {
    gain2 |= static_cast<uint32_t>(encode_pga_gain_(this->current_frontend_.pga_gain)) << ((i + 2) * 4);
  }

  if (!this->write_register_(REG_GAIN1, gain1))
    return false;
  if (!this->write_register_(REG_GAIN2, gain2))
    return false;

  // RMS gain trim and offset correction for current channels
  int16_t current_trim = gain_to_register_(this->current_reference_);
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (!this->write_register_(REG_RMSGN[i], static_cast<uint16_t>(current_trim)))
      return false;
    uint32_t rmsos_val = static_cast<uint32_t>(this->saved_rmsos_[i]) & 0xFFFFFF;
    if (!this->write_register_(REG_RMSOS[i], rmsos_val))
      return false;
  }

  // RMS gain trim for voltage
  int16_t voltage_trim = gain_to_register_(this->voltage_reference_);
  if (!this->write_register_(REG_RMSGN_V, static_cast<uint16_t>(voltage_trim)))
    return false;
  if (!this->write_register_(REG_RMSOS_V, 0))
    return false;

  // Active power gain trim per channel
  int16_t power_trim = gain_to_register_(this->power_reference_);
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (!this->write_register_(REG_WATTGN[i], static_cast<uint16_t>(power_trim)))
      return false;
  }

  // Energy pulse divisor
  if (!this->write_register_(REG_CFDIV, this->cfdiv_))
    return false;

  ESP_LOGD(TAG, "Calibration applied: GAIN1=0x%06X GAIN2=0x%06X CFDIV=0x%03X", gain1, gain2, this->cfdiv_);
  return true;
}

void BL0906Component::recalculate_scales_() {
  const float pin_rms_full_scale = BL0906_ADC_RMS_FULL_SCALE_VOLTS;

  if (this->voltage_frontend_.sample_res > 0.0f && this->voltage_frontend_.pga_gain > 0) {
    const float divider_ratio =
        (this->voltage_frontend_.load_res + this->voltage_frontend_.sample_res) / this->voltage_frontend_.sample_res;
    this->voltage_scale_ =
        (pin_rms_full_scale / static_cast<float>(this->voltage_frontend_.pga_gain)) *
        divider_ratio * this->voltage_frontend_.sample_ratio;
  } else {
    this->voltage_scale_ = 1.0f;
  }

  if (this->current_frontend_.sample_res > 0.0f && this->current_frontend_.pga_gain > 0) {
    this->current_scale_ =
        (pin_rms_full_scale / static_cast<float>(this->current_frontend_.pga_gain)) *
        this->current_frontend_.sample_ratio / this->current_frontend_.sample_res;
  } else {
    this->current_scale_ = 1.0f;
  }

  this->power_scale_ = this->voltage_scale_ * this->current_scale_;
}

// --- PGA Helpers ---

uint8_t BL0906Component::encode_pga_gain_(uint8_t gain) {
  switch (gain) {
    case 1:  return 0x0;
    case 2:  return 0x1;
    case 8:  return 0x2;
    case 16: return 0x3;
    default: return 0x0;
  }
}

int16_t BL0906Component::gain_to_register_(float gain) {
  int32_t reg = static_cast<int32_t>(std::lround((gain - 1.0f) * 65536.0f));
  if (reg > 32767)
    reg = 32767;
  if (reg < -32768)
    reg = -32768;
  return static_cast<int16_t>(reg);
}

// --- Conversions ---

float BL0906Component::convert_voltage_(uint32_t raw) {
  return static_cast<float>(raw) * this->voltage_scale_ / BL0906_RMS_DIVISOR;
}

float BL0906Component::convert_current_(uint32_t raw) {
  return static_cast<float>(raw) * this->current_scale_ / BL0906_RMS_DIVISOR;
}

float BL0906Component::convert_power_(int32_t raw) {
  return static_cast<float>(raw) * this->power_scale_ / BL0906_RMS_DIVISOR;
}

float BL0906Component::convert_energy_(uint32_t raw) {
  return static_cast<float>(raw) * this->energy_reference_;
}

float BL0906Component::convert_frequency_(uint32_t raw) {
  if (raw == 0)
    return 0.0f;
  return 10000000.0f / static_cast<float>(raw);
}

float BL0906Component::convert_temperature_(uint32_t raw) {
  return (static_cast<float>(raw & 0x3FF) - 64.0f) * 12.5f / 59.0f - 40.0f;
}

// --- Reset ---

void BL0906Component::reset_energy() {
  ESP_LOGI(TAG, "Resetting energy counters");
  this->initialized_ = false;

  if (!this->chip_init_()) {
    ESP_LOGE(TAG, "Reinitialization after reset failed");
    this->mark_failed();
    return;
  }

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->accumulated_energy_[i] = 0.0f;
    this->last_cf_cnt_[i] = 0;
    this->last_cf_cnt_valid_[i] = false;
  }
  this->accumulated_energy_total_ = 0.0f;
  this->last_cf_cnt_total_ = 0;
  this->last_cf_cnt_total_valid_ = false;
  this->energy_state_dirty_ = true;
  this->save_energy_state_(true);
  global_preferences->sync();
}

// --- Auto-Zero Calibration ---

void BL0906Component::calibrate_zero() {
  ESP_LOGI(TAG, "Starting auto-zero calibration (ensure NO LOAD on all channels)");

  static constexpr uint8_t NUM_SAMPLES = 4;
  int32_t candidate_rmsos[NUM_CHANNELS];
  uint32_t avg_samples[NUM_CHANNELS];

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    candidate_rmsos[i] = this->saved_rmsos_[i];
    avg_samples[i] = 0;
  }

  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK)) {
    ESP_LOGE(TAG, "Failed to unlock write protection for calibration");
    return;
  }

  // Clear existing RMSOS so we read the uncorrected noise floor
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->write_register_(REG_RMSOS[i], 0);
  }

  // Yield to other tasks briefly instead of a long blocking delay
  delay(50);
  App.feed_wdt();

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    uint64_t sum = 0;
    uint8_t valid = 0;

    for (uint8_t s = 0; s < NUM_SAMPLES; s++) {
      uint32_t raw;
      if (this->read_register_(REG_I_RMS[ch], raw)) {
        sum += raw;
        valid++;
      }
      delay(20);
      App.feed_wdt();
    }

    if (valid == 0) {
      ESP_LOGW(TAG, "Ch%u: no valid reads, skipping", ch + 1);
      for (uint8_t restore = 0; restore < NUM_CHANNELS; restore++) {
        if (!this->write_register_(REG_RMSOS[restore], static_cast<uint32_t>(this->saved_rmsos_[restore]) & 0xFFFFFF)) {
          ESP_LOGW(TAG, "Ch%u: failed to restore previous RMSOS", restore + 1);
        }
      }
      return;
    }

    uint32_t avg_raw = static_cast<uint32_t>(sum / valid);
    avg_samples[ch] = avg_raw;
    const float avg_current = this->convert_current_(avg_raw);
    if (avg_current > ZERO_CALIBRATION_MAX_CURRENT_A) {
      ESP_LOGW(TAG, "Ch%u: refusing to save calibration, measured %.4f A exceeds no-load threshold %.4f A", ch + 1,
               avg_current, ZERO_CALIBRATION_MAX_CURRENT_A);
      for (uint8_t restore = 0; restore < NUM_CHANNELS; restore++) {
        if (!this->write_register_(REG_RMSOS[restore], static_cast<uint32_t>(this->saved_rmsos_[restore]) & 0xFFFFFF)) {
          ESP_LOGW(TAG, "Ch%u: failed to restore previous RMSOS", restore + 1);
        }
      }
      return;
    }
    int32_t rmsos = -static_cast<int32_t>((static_cast<uint64_t>(avg_raw) * avg_raw) / 256);

    if (rmsos < -8388608)
      rmsos = -8388608;

    candidate_rmsos[ch] = rmsos;

    uint32_t rmsos_reg = static_cast<uint32_t>(rmsos) & 0xFFFFFF;
    if (!this->write_register_(REG_RMSOS[ch], rmsos_reg)) {
      ESP_LOGW(TAG, "Ch%u: failed to write RMSOS", ch + 1);
      for (uint8_t restore = 0; restore < NUM_CHANNELS; restore++) {
        if (!this->write_register_(REG_RMSOS[restore], static_cast<uint32_t>(this->saved_rmsos_[restore]) & 0xFFFFFF)) {
          ESP_LOGW(TAG, "Ch%u: failed to restore previous RMSOS", restore + 1);
        }
      }
      return;
    }

    ESP_LOGI(TAG, "Ch%u: avg_raw=%u, RMSOS=%d (0x%06X)", ch + 1, avg_raw, rmsos, rmsos_reg);
  }

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    this->saved_rmsos_[ch] = candidate_rmsos[ch];
    ESP_LOGD(TAG, "Ch%u: saved avg_raw=%u, RMSOS=%d", ch + 1, avg_samples[ch], candidate_rmsos[ch]);
  }

  this->rmsos_pref_.save(&this->saved_rmsos_);
  global_preferences->sync();

  ESP_LOGI(TAG, "Auto-zero calibration complete, saved to flash");
}

}  // namespace bl0906
}  // namespace esphome

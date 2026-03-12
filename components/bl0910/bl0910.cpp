#include "bl0910.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0910 {

static const char *const TAG = "bl0910";
static constexpr float BL0910_INPUT_FULL_SCALE_PEAK_VOLTS = 0.7f;
static constexpr float BL0910_PEAK_TO_RMS = 0.7071067811865476f;
static constexpr float BL0910_RMS_DIVISOR = static_cast<float>(1 << 22);

void BL0910Component::set_voltage_load_res(float value) {
  this->voltage_frontend_.load_res = value;
  this->recalculate_scales_();
}

void BL0910Component::set_voltage_sample_res(float value) {
  this->voltage_frontend_.sample_res = value;
  this->recalculate_scales_();
}

void BL0910Component::set_voltage_sample_ratio(float value) {
  this->voltage_frontend_.sample_ratio = value;
  this->recalculate_scales_();
}

void BL0910Component::set_voltage_pga_gain(uint8_t value) {
  this->voltage_frontend_.pga_gain = value;
  this->recalculate_scales_();
}

void BL0910Component::set_current_sample_res(float value) {
  this->current_frontend_.sample_res = value;
  this->recalculate_scales_();
}

void BL0910Component::set_current_sample_ratio(float value) {
  this->current_frontend_.sample_ratio = value;
  this->recalculate_scales_();
}

void BL0910Component::set_current_pga_gain(uint8_t value) {
  this->current_frontend_.pga_gain = value;
  this->recalculate_scales_();
}

void BL0910Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BL0910...");
  this->spi_setup();
  this->recalculate_scales_();

  this->cached_voltage_rms_ = NAN;
  this->cached_total_active_power_ = NAN;
  this->cached_frequency_ = NAN;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->cached_voltage_rms_by_channel_[i] = NAN;
    this->cached_current_rms_[i] = NAN;
    this->cached_active_power_[i] = NAN;
  }

  if (this->irq_pin_ != nullptr) {
    this->irq_pin_->setup();
  }

  this->hardware_reset_();

  if (!this->chip_init_()) {
    ESP_LOGE(TAG, "BL0910 initialization failed!");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "BL0910 initialized successfully");
}

void BL0910Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BL0910:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);

  const char *mode_str;
  switch (this->mode_) {
    case MODE_1U10I:
      mode_str = "1U10I";
      break;
    case MODE_5U5I:
      mode_str = "5U5I";
      break;
    case MODE_3U6I:
      mode_str = "3U6I";
      break;
    default:
      mode_str = "Unknown";
      break;
  }

  ESP_LOGCONFIG(TAG, "  Mode: %s", mode_str);
  ESP_LOGCONFIG(TAG, "  Line Frequency: %s",
                this->line_frequency_ == LINE_FREQUENCY_50HZ ? "50Hz" : "60Hz");
  ESP_LOGCONFIG(TAG, "  Voltage Frontend: load_res=%.3f ohm sample_res=%.3f ohm sample_ratio=%.6f pga=%ux",
                this->voltage_frontend_.load_res, this->voltage_frontend_.sample_res,
                this->voltage_frontend_.sample_ratio, this->voltage_frontend_.pga_gain);
  ESP_LOGCONFIG(TAG, "  Current Frontend: sample_res=%.6f ohm sample_ratio=%.6f pga=%ux",
                this->current_frontend_.sample_res, this->current_frontend_.sample_ratio,
                this->current_frontend_.pga_gain);
  ESP_LOGCONFIG(TAG, "  Derived Voltage Scale: %.6f V_RMS FS", this->voltage_scale_);
  ESP_LOGCONFIG(TAG, "  Derived Current Scale: %.6f A_RMS FS", this->current_scale_);
  ESP_LOGCONFIG(TAG, "  Derived Power Scale: %.6f W FS", this->power_scale_);
  ESP_LOGCONFIG(TAG, "  Voltage Trim (RMSGN): %.6f", this->voltage_reference_);
  ESP_LOGCONFIG(TAG, "  Current Trim (RMSGN): %.6f", this->current_reference_);
  ESP_LOGCONFIG(TAG, "  Power Trim (WATTGN): %.6f", this->power_reference_);
  ESP_LOGCONFIG(TAG, "  Energy Reference: %.6f", this->energy_reference_);
  ESP_LOGCONFIG(TAG, "  CFDIV: 0x%03X", this->cfdiv_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Total Energy", this->total_energy_sensor_);

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->voltage_channel_sensors_[i] == nullptr && this->current_sensors_[i] == nullptr &&
        this->power_sensors_[i] == nullptr && this->energy_sensors_[i] == nullptr &&
        this->power_factor_sensors_[i] == nullptr) {
      continue;
    }

    ESP_LOGCONFIG(TAG, "  Channel %u:", i + 1);
    if (this->is_valid_measurement_channel_(i)) {
      ESP_LOGCONFIG(TAG, "    Mapping: I%u x U%u", this->get_current_input_index_(i) + 1,
                    this->get_voltage_input_index_(i) + 1);
    } else {
      ESP_LOGW(TAG, "    Mapping: unavailable in %s mode", mode_str);
    }
    LOG_SENSOR("    ", "Voltage", this->voltage_channel_sensors_[i]);
    LOG_SENSOR("    ", "Current", this->current_sensors_[i]);
    LOG_SENSOR("    ", "Power", this->power_sensors_[i]);
    LOG_SENSOR("    ", "Energy", this->energy_sensors_[i]);
    LOG_SENSOR("    ", "Power Factor", this->power_factor_sensors_[i]);
  }

  if (this->error_reason_ != nullptr) {
    ESP_LOGE(TAG, "  Init Error: %s (register 0x%02X)", this->error_reason_, this->last_failed_register_);
    ESP_LOGE(TAG, "  Last RX: %02X %02X %02X %02X", this->last_rx_data_[0], this->last_rx_data_[1],
             this->last_rx_data_[2], this->last_rx_data_[3]);
  }
}

void BL0910Component::update() {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Not initialized, skipping update");
    return;
  }

  float rms_cache[11];
  bool rms_loaded[11];
  for (uint8_t i = 0; i < 11; i++) {
    rms_cache[i] = NAN;
    rms_loaded[i] = false;
  }

  auto read_input_rms = [&](int8_t input_index, bool voltage_role, float &value) -> bool {
    if (input_index < 0 || input_index > 10)
      return false;

    if (!rms_loaded[input_index]) {
      uint32_t raw;
      if (!this->read_register_(REG_RMS_1 + input_index, raw))
        return false;

      rms_cache[input_index] = voltage_role ? this->convert_voltage_(raw) : this->convert_current_(raw);
      rms_loaded[input_index] = true;
    }

    value = rms_cache[input_index];
    return true;
  };

  this->cached_voltage_rms_ = NAN;
  int8_t primary_voltage_input = this->get_primary_voltage_input_index_();
  if (primary_voltage_input >= 0) {
    float voltage;
    if (read_input_rms(primary_voltage_input, true, voltage)) {
      this->cached_voltage_rms_ = voltage;
      if (this->voltage_sensor_ != nullptr)
        this->voltage_sensor_->publish_state(voltage);
    }
  }

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->cached_voltage_rms_by_channel_[i] = NAN;
    this->cached_current_rms_[i] = NAN;

    const int8_t voltage_input = this->get_voltage_input_index_(i);
    if (voltage_input >= 0) {
      float voltage;
      if (read_input_rms(voltage_input, true, voltage)) {
        this->cached_voltage_rms_by_channel_[i] = voltage;
        if (this->voltage_channel_sensors_[i] != nullptr)
          this->voltage_channel_sensors_[i]->publish_state(voltage);
      }
    }

    const int8_t current_input = this->get_current_input_index_(i);
    if (current_input >= 0) {
      float current;
      if (read_input_rms(current_input, false, current)) {
        this->cached_current_rms_[i] = current;
        if (this->current_sensors_[i] != nullptr)
          this->current_sensors_[i]->publish_state(current);
      }
    }
  }

  uint32_t raw;

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->cached_active_power_[i] = NAN;
    if (!this->is_valid_measurement_channel_(i))
      continue;

    if (this->read_register_(REG_WATT_1 + i, raw)) {
      int32_t signed_raw = static_cast<int32_t>(raw);
      if (signed_raw & 0x800000)
        signed_raw |= static_cast<int32_t>(0xFF000000);
      this->cached_active_power_[i] = this->convert_power_(signed_raw);
      if (this->power_sensors_[i] != nullptr)
        this->power_sensors_[i]->publish_state(this->cached_active_power_[i]);
    }
  }

  this->cached_total_active_power_ = NAN;
  if (this->read_register_(REG_WATT_TOTAL, raw)) {
    int32_t signed_raw = static_cast<int32_t>(raw);
    if (signed_raw & 0x800000)
      signed_raw |= static_cast<int32_t>(0xFF000000);
    this->cached_total_active_power_ = this->convert_power_(signed_raw);
    if (this->total_power_sensor_ != nullptr)
      this->total_power_sensor_->publish_state(this->cached_total_active_power_);
  }

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->energy_sensors_[i] == nullptr || !this->is_valid_measurement_channel_(i))
      continue;

    if (this->read_register_(REG_CF_CNT_1 + i, raw)) {
      uint32_t delta;
      if (this->last_cf_cnt_[i] == 0 && this->accumulated_energy_[i] == 0.0f) {
        delta = 0;
      } else if (raw >= this->last_cf_cnt_[i]) {
        delta = raw - this->last_cf_cnt_[i];
      } else {
        delta = (0xFFFFFF - this->last_cf_cnt_[i]) + raw + 1;
      }
      this->last_cf_cnt_[i] = raw;
      this->accumulated_energy_[i] += this->convert_energy_(delta);
      this->energy_sensors_[i]->publish_state(this->accumulated_energy_[i]);
    }
  }

  if (this->total_energy_sensor_ != nullptr && this->read_register_(REG_CF_CNT_TOTAL, raw)) {
    uint32_t delta;
    if (this->last_cf_cnt_total_ == 0 && this->accumulated_energy_total_ == 0.0f) {
      delta = 0;
    } else if (raw >= this->last_cf_cnt_total_) {
      delta = raw - this->last_cf_cnt_total_;
    } else {
      delta = (0xFFFFFF - this->last_cf_cnt_total_) + raw + 1;
    }
    this->last_cf_cnt_total_ = raw;
    this->accumulated_energy_total_ += this->convert_energy_(delta);
    this->total_energy_sensor_->publish_state(this->accumulated_energy_total_);
  }

  if (this->read_register_(REG_PERIOD, raw)) {
    raw &= 0x0FFFFF;
    if (raw > 0) {
      this->cached_frequency_ = this->convert_frequency_(raw);
      if (this->frequency_sensor_ != nullptr)
        this->frequency_sensor_->publish_state(this->cached_frequency_);
    }
  }

  if (this->temperature_sensor_ != nullptr && this->read_register_(REG_TPS1, raw)) {
    this->temperature_sensor_->publish_state(this->convert_temperature_(raw));
  }

  bool need_pf = false;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->power_factor_sensors_[i] != nullptr && this->is_valid_measurement_channel_(i)) {
      need_pf = true;
      break;
    }
  }

  if (need_pf && this->read_register_(REG_PF, raw)) {
    int32_t signed_raw = static_cast<int32_t>(raw);
    if (signed_raw & 0x800000)
      signed_raw |= static_cast<int32_t>(0xFF000000);
    float pf = this->convert_power_factor_(signed_raw);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      if (this->power_factor_sensors_[i] != nullptr && this->is_valid_measurement_channel_(i))
        this->power_factor_sensors_[i]->publish_state(pf);
    }
  }
}

bool BL0910Component::read_register_(uint8_t reg, uint32_t &value) {
  for (uint8_t retry = 0; retry < SPI_RETRY_COUNT; retry++) {
    this->enable();
    this->write_byte(SPI_READ_CMD);
    this->write_byte(reg);
    uint8_t data_h = this->read_byte();
    uint8_t data_m = this->read_byte();
    uint8_t data_l = this->read_byte();
    uint8_t rx_checksum = this->read_byte();
    this->disable();

    uint8_t expected = ~(SPI_READ_CMD + reg + data_h + data_m + data_l) & 0xFF;

    if (rx_checksum == expected) {
      value = (static_cast<uint32_t>(data_h) << 16) | (static_cast<uint32_t>(data_m) << 8) |
              static_cast<uint32_t>(data_l);
      return true;
    }

    ESP_LOGW(TAG, "Read checksum mismatch reg 0x%02X attempt %u: got 0x%02X expected 0x%02X (data: %02X %02X %02X)",
             reg, retry + 1, rx_checksum, expected, data_h, data_m, data_l);

    this->last_rx_data_[0] = data_h;
    this->last_rx_data_[1] = data_m;
    this->last_rx_data_[2] = data_l;
    this->last_rx_data_[3] = rx_checksum;
  }

  this->error_reason_ = "Read checksum failed after retries";
  this->last_failed_register_ = reg;
  return false;
}

bool BL0910Component::write_register_(uint8_t reg, uint32_t value) {
  uint8_t data_h = (value >> 16) & 0xFF;
  uint8_t data_m = (value >> 8) & 0xFF;
  uint8_t data_l = value & 0xFF;
  uint8_t checksum = ~(SPI_WRITE_CMD + reg + data_h + data_m + data_l) & 0xFF;

  this->enable();
  this->write_byte(SPI_WRITE_CMD);
  this->write_byte(reg);
  this->write_byte(data_h);
  this->write_byte(data_m);
  this->write_byte(data_l);
  this->write_byte(checksum);
  this->disable();

  return true;
}

bool BL0910Component::write_user_register_(uint8_t reg, uint32_t value) {
  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;
  return this->write_register_(reg, value);
}

void BL0910Component::reset_spi_() {
  this->enable();
  for (uint8_t i = 0; i < 6; i++) {
    this->write_byte(0xFF);
  }
  this->disable();
}

void BL0910Component::hardware_reset_() {
  if (this->reset_pin_ == nullptr)
    return;

  ESP_LOGD(TAG, "Hardware reset");
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  delay(20);
  this->reset_pin_->digital_write(true);
  delay(100);
}

bool BL0910Component::chip_init_() {
  ESP_LOGD(TAG, "Chip init sequence starting");
  this->recalculate_scales_();

  this->reset_spi_();
  delay(10);

  uint32_t raw;
  if (!this->read_register_(REG_TPS1, raw)) {
    ESP_LOGE(TAG, "Communication check failed (TPS1 read)");
    this->error_reason_ = "Initial TPS1 read failed";
    this->last_failed_register_ = REG_TPS1;
    return false;
  }
  ESP_LOGD(TAG, "TPS1 = 0x%06X -- comms OK", raw);

  if (!this->write_user_register_(REG_SOFT_RESET, SOFT_RESET_VALUE))
    return false;

  delay(50);

  this->reset_spi_();
  delay(10);

  if (!this->configure_mode_())
    return false;

  if (!this->write_user_register_(REG_MODE1, 1u << 22))
    return false;

  if (!this->apply_calibration_())
    return false;

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->last_cf_cnt_[i] = 0;
  }
  this->last_cf_cnt_total_ = 0;
  this->error_reason_ = nullptr;
  this->last_failed_register_ = 0;
  this->initialized_ = true;
  return true;
}

bool BL0910Component::configure_mode_() {
  uint32_t mode_val = 0;
  if (this->mode_ != MODE_1U10I)
    mode_val |= (1u << 19);

  ESP_LOGD(TAG, "MODE register = 0x%06X", mode_val);
  return this->write_user_register_(REG_MODE, mode_val);
}

bool BL0910Component::apply_calibration_() {
  uint32_t gain1 = 0;
  uint32_t gain2 = 0;

  for (uint8_t input = 0; input < 11; input++) {
    uint8_t gain = 1;
    if (this->is_voltage_input_(input)) {
      gain = this->voltage_frontend_.pga_gain;
    } else if (this->is_current_input_(input)) {
      gain = this->current_frontend_.pga_gain;
    }

    uint8_t encoded = this->encode_pga_gain_(gain);
    if (input == 10) {
      gain1 |= encoded;
    } else if (input < 5) {
      gain1 |= static_cast<uint32_t>(encoded) << ((input + 1) * 4);
    } else {
      gain2 |= static_cast<uint32_t>(encoded) << ((input - 5) * 4);
    }
  }

  if (!this->write_user_register_(REG_GAIN1, gain1))
    return false;
  if (!this->write_user_register_(REG_GAIN2, gain2))
    return false;

  for (uint8_t input = 0; input < 11; input++) {
    float trim = 1.0f;
    if (this->is_voltage_input_(input)) {
      trim = this->voltage_reference_;
    } else if (this->is_current_input_(input)) {
      trim = this->current_reference_;
    }

    if (!this->write_user_register_(REG_RMSGN_1 + input,
                                    static_cast<uint16_t>(this->gain_to_register_(trim)))) {
      return false;
    }
    if (!this->write_user_register_(REG_RMSOS_1 + input, 0))
      return false;
  }

  for (uint8_t channel = 0; channel < NUM_CHANNELS; channel++) {
    int16_t watt_gain = this->is_valid_measurement_channel_(channel) ? this->gain_to_register_(this->power_reference_) : 0;
    if (!this->write_user_register_(REG_WATTGN_1 + channel, static_cast<uint16_t>(watt_gain)))
      return false;
    if (!this->write_user_register_(REG_WATTOS_1 + channel, 0))
      return false;
  }

  if (!this->write_user_register_(REG_CFDIV, this->cfdiv_))
    return false;

  ESP_LOGD(TAG, "Applied calibration: GAIN1=0x%06X GAIN2=0x%06X CFDIV=0x%03X", gain1, gain2, this->cfdiv_);
  return true;
}

void BL0910Component::hardware_reset() {
  ESP_LOGI(TAG, "Performing hardware reset");
  this->initialized_ = false;
  this->hardware_reset_();
  if (!this->chip_init_()) {
    ESP_LOGE(TAG, "Reinitialization after hardware reset failed");
    this->mark_failed();
  }
}

void BL0910Component::reinitialize() {
  ESP_LOGI(TAG, "Reinitializing (chip_init only)");
  this->initialized_ = false;
  if (!this->chip_init_()) {
    ESP_LOGE(TAG, "Reinitialization failed");
    this->mark_failed();
  }
}

bool BL0910Component::write_irq_mask(uint32_t mask) { return this->write_user_register_(REG_MASK1, mask); }

void BL0910Component::recalculate_scales_() {
  const float pin_rms_full_scale = BL0910_INPUT_FULL_SCALE_PEAK_VOLTS * BL0910_PEAK_TO_RMS;

  if (this->voltage_frontend_.sample_res > 0.0f && this->voltage_frontend_.pga_gain > 0) {
    const float divider_ratio =
        (this->voltage_frontend_.load_res + this->voltage_frontend_.sample_res) / this->voltage_frontend_.sample_res;
    this->voltage_scale_ =
        (pin_rms_full_scale / static_cast<float>(this->voltage_frontend_.pga_gain)) * divider_ratio * this->voltage_frontend_.sample_ratio;
  } else {
    this->voltage_scale_ = 1.0f;
  }

  if (this->current_frontend_.sample_res > 0.0f && this->current_frontend_.pga_gain > 0) {
    this->current_scale_ =
        (pin_rms_full_scale / static_cast<float>(this->current_frontend_.pga_gain)) * this->current_frontend_.sample_ratio /
        this->current_frontend_.sample_res;
  } else {
    this->current_scale_ = 1.0f;
  }

  this->power_scale_ = this->voltage_scale_ * this->current_scale_;
}

bool BL0910Component::is_valid_measurement_channel_(uint8_t channel) const {
  return this->get_current_input_index_(channel) >= 0 && this->get_voltage_input_index_(channel) >= 0;
}

int8_t BL0910Component::get_current_input_index_(uint8_t channel) const {
  if (channel >= NUM_CHANNELS)
    return -1;

  switch (this->mode_) {
    case MODE_1U10I:
      return channel;
    case MODE_5U5I:
      return channel < 5 ? channel : -1;
    case MODE_3U6I: {
      static const int8_t MAP[NUM_CHANNELS] = {-1, 1, 2, 3, -1, -1, 0, 4, 5, -1};
      return MAP[channel];
    }
    default:
      return -1;
  }
}

int8_t BL0910Component::get_voltage_input_index_(uint8_t channel) const {
  if (channel >= NUM_CHANNELS)
    return -1;

  switch (this->mode_) {
    case MODE_1U10I:
      return 10;
    case MODE_5U5I: {
      static const int8_t MAP[NUM_CHANNELS] = {10, 9, 8, 7, 6, -1, -1, -1, -1, -1};
      return MAP[channel];
    }
    case MODE_3U6I: {
      static const int8_t MAP[NUM_CHANNELS] = {-1, 9, 8, 7, -1, -1, 9, 8, 7, -1};
      return MAP[channel];
    }
    default:
      return -1;
  }
}

bool BL0910Component::is_current_input_(uint8_t input_index) const {
  switch (this->mode_) {
    case MODE_1U10I:
      return input_index <= 9;
    case MODE_5U5I:
      return input_index <= 4;
    case MODE_3U6I:
      return input_index <= 5;
    default:
      return false;
  }
}

bool BL0910Component::is_voltage_input_(uint8_t input_index) const {
  switch (this->mode_) {
    case MODE_1U10I:
      return input_index == 10;
    case MODE_5U5I:
      return input_index >= 6 && input_index <= 10;
    case MODE_3U6I:
      return input_index >= 7 && input_index <= 9;
    default:
      return false;
  }
}

int8_t BL0910Component::get_primary_voltage_input_index_() const {
  switch (this->mode_) {
    case MODE_1U10I:
    case MODE_5U5I:
      return 10;
    case MODE_3U6I:
      return 9;
    default:
      return -1;
  }
}

uint8_t BL0910Component::encode_pga_gain_(uint8_t gain) {
  switch (gain) {
    case 1:
      return 0x0;
    case 2:
      return 0x1;
    case 8:
      return 0x2;
    case 16:
      return 0x3;
    default:
      return 0x0;
  }
}

int16_t BL0910Component::gain_to_register_(float gain) {
  int32_t reg = static_cast<int32_t>(std::lround((gain - 1.0f) * 65536.0f));
  if (reg > 32767)
    reg = 32767;
  if (reg < -32768)
    reg = -32768;
  return static_cast<int16_t>(reg);
}

float BL0910Component::convert_voltage_(uint32_t raw) {
  return static_cast<float>(raw) * this->voltage_scale_ / BL0910_RMS_DIVISOR;
}

float BL0910Component::convert_current_(uint32_t raw) {
  return static_cast<float>(raw) * this->current_scale_ / BL0910_RMS_DIVISOR;
}

float BL0910Component::convert_power_(int32_t raw) {
  return static_cast<float>(raw) * this->power_scale_ / BL0910_RMS_DIVISOR;
}

float BL0910Component::convert_energy_(uint32_t raw) { return static_cast<float>(raw) * this->energy_reference_; }

float BL0910Component::convert_frequency_(uint32_t raw) {
  if (raw == 0)
    return 0.0f;
  return 10000000.0f / static_cast<float>(raw);
}

float BL0910Component::convert_temperature_(uint32_t raw) {
  return (static_cast<float>(raw & 0x3FF) - 64.0f) * 12.5f / 59.0f - 40.0f;
}

float BL0910Component::convert_power_factor_(int32_t raw) {
  return static_cast<float>(raw) / static_cast<float>(1 << 23);
}

}  // namespace bl0910
}  // namespace esphome

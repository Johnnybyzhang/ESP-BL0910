#include "bl0910.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0910 {

static const char *const TAG = "bl0910";

void BL0910Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BL0910...");
  this->spi_setup();

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
  ESP_LOGCONFIG(TAG, "  Voltage Reference: %.6f", this->voltage_reference_);
  ESP_LOGCONFIG(TAG, "  Current Reference: %.6f", this->current_reference_);
  ESP_LOGCONFIG(TAG, "  Power Reference: %.6f", this->power_reference_);
  ESP_LOGCONFIG(TAG, "  Energy Reference: %.6f", this->energy_reference_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Total Energy", this->total_energy_sensor_);

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->current_sensors_[i] != nullptr || this->power_sensors_[i] != nullptr ||
        this->energy_sensors_[i] != nullptr || this->power_factor_sensors_[i] != nullptr) {
      ESP_LOGCONFIG(TAG, "  Channel %u:", i + 1);
      LOG_SENSOR("    ", "Current", this->current_sensors_[i]);
      LOG_SENSOR("    ", "Power", this->power_sensors_[i]);
      LOG_SENSOR("    ", "Energy", this->energy_sensors_[i]);
      LOG_SENSOR("    ", "Power Factor", this->power_factor_sensors_[i]);
    }
  }

  if (this->error_reason_ != nullptr) {
    ESP_LOGE(TAG, "  Init Error: %s (register 0x%02X)", this->error_reason_, this->last_failed_register_);
    ESP_LOGE(TAG, "  Last RX: %02X %02X %02X %02X",
             this->last_rx_data_[0], this->last_rx_data_[1],
             this->last_rx_data_[2], this->last_rx_data_[3]);
  }
}

void BL0910Component::update() {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Not initialized, skipping update");
    return;
  }

  uint32_t raw;

  // Voltage: RMS[11] at 0x16
  if (this->voltage_sensor_ != nullptr) {
    if (this->read_register_(REG_RMS_11, raw)) {
      this->voltage_sensor_->publish_state(this->convert_voltage_(raw));
    }
  }

  // Current per channel: RMS[1-10] at 0x0C-0x15
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->current_sensors_[i] != nullptr) {
      if (this->read_register_(REG_RMS_1 + i, raw)) {
        this->current_sensors_[i]->publish_state(this->convert_current_(raw));
      }
    }
  }

  // Active power per channel: WATT[1-10] at 0x22-0x2B (signed 24-bit)
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->power_sensors_[i] != nullptr) {
      if (this->read_register_(REG_WATT_1 + i, raw)) {
        int32_t signed_raw = static_cast<int32_t>(raw);
        if (signed_raw & 0x800000)
          signed_raw |= static_cast<int32_t>(0xFF000000);
        this->power_sensors_[i]->publish_state(this->convert_power_(signed_raw));
      }
    }
  }

  // Total active power: WATT_TOTAL at 0x2C (signed 24-bit)
  if (this->total_power_sensor_ != nullptr) {
    if (this->read_register_(REG_WATT_TOTAL, raw)) {
      int32_t signed_raw = static_cast<int32_t>(raw);
      if (signed_raw & 0x800000)
        signed_raw |= static_cast<int32_t>(0xFF000000);
      this->total_power_sensor_->publish_state(this->convert_power_(signed_raw));
    }
  }

  // Energy counters per channel: CF_CNT[1-10] at 0x2F-0x38 (delta accumulation)
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->energy_sensors_[i] != nullptr) {
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
  }

  // Total energy counter: CF_CNT_TOTAL at 0x39
  if (this->total_energy_sensor_ != nullptr) {
    if (this->read_register_(REG_CF_CNT_TOTAL, raw)) {
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
  }

  // Line frequency: PERIOD at 0x4E
  if (this->frequency_sensor_ != nullptr) {
    if (this->read_register_(REG_PERIOD, raw)) {
      raw &= 0x0FFFFF;  // 20-bit value
      if (raw > 0) {
        this->frequency_sensor_->publish_state(this->convert_frequency_(raw));
      }
    }
  }

  // Internal temperature: TPS1 at 0x5E
  if (this->temperature_sensor_ != nullptr) {
    if (this->read_register_(REG_TPS1, raw)) {
      this->temperature_sensor_->publish_state(this->convert_temperature_(raw));
    }
  }

  // Power factor: PF at 0x4A (signed 24-bit, for VAR_I_SEL selected channel only)
  bool need_pf = false;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->power_factor_sensors_[i] != nullptr) {
      need_pf = true;
      break;
    }
  }
  if (need_pf) {
    if (this->read_register_(REG_PF, raw)) {
      int32_t signed_raw = static_cast<int32_t>(raw);
      if (signed_raw & 0x800000)
        signed_raw |= static_cast<int32_t>(0xFF000000);
      float pf = this->convert_power_factor_(signed_raw);
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (this->power_factor_sensors_[i] != nullptr)
          this->power_factor_sensors_[i]->publish_state(pf);
      }
    }
  }
}

// --- SPI Communication ---

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
      value = (static_cast<uint32_t>(data_h) << 16) |
              (static_cast<uint32_t>(data_m) << 8) |
              static_cast<uint32_t>(data_l);
      return true;
    }

    ESP_LOGW(TAG, "Read checksum mismatch reg 0x%02X attempt %u: got 0x%02X expected 0x%02X "
                  "(data: %02X %02X %02X)",
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

void BL0910Component::reset_spi_() {
  this->enable();
  for (uint8_t i = 0; i < 6; i++) {
    this->write_byte(0xFF);
  }
  this->disable();
}

// --- Initialization ---

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

  this->reset_spi_();
  delay(10);

  // Verify communication by reading TPS1
  uint32_t raw;
  if (!this->read_register_(REG_TPS1, raw)) {
    ESP_LOGE(TAG, "Communication check failed (TPS1 read)");
    this->error_reason_ = "Initial TPS1 read failed";
    this->last_failed_register_ = REG_TPS1;
    return false;
  }
  ESP_LOGD(TAG, "TPS1 = 0x%06X -- comms OK", raw);

  // Unlock -> soft reset
  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;
  if (!this->write_register_(REG_SOFT_RESET, SOFT_RESET_VALUE))
    return false;

  delay(50);

  // SPI reset after soft reset, then unlock again
  this->reset_spi_();
  delay(10);

  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;

  // Configure operating mode
  if (!this->configure_mode_())
    return false;

  // Enable HPF on fast RMS: MODE1 bit[22] = 1
  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;
  if (!this->write_register_(REG_MODE1, 1u << 22))
    return false;

  this->initialized_ = true;
  return true;
}

bool BL0910Component::configure_mode_() {
  if (!this->write_register_(REG_USR_WRPROT, USR_WRPROT_UNLOCK))
    return false;

  uint32_t mode_val = 0;
  if (this->mode_ != MODE_1U10I)
    mode_val |= (1u << 19);

  ESP_LOGD(TAG, "MODE register = 0x%06X", mode_val);
  return this->write_register_(REG_MODE, mode_val);
}

// --- Value Conversions ---

float BL0910Component::convert_voltage_(uint32_t raw) {
  return static_cast<float>(raw) * this->voltage_reference_ / static_cast<float>(1 << 22);
}

float BL0910Component::convert_current_(uint32_t raw) {
  return static_cast<float>(raw) * this->current_reference_ / static_cast<float>(1 << 22);
}

float BL0910Component::convert_power_(int32_t raw) {
  return static_cast<float>(raw) * this->power_reference_ / static_cast<float>(1 << 22);
}

float BL0910Component::convert_energy_(uint32_t raw) {
  return static_cast<float>(raw) * this->energy_reference_;
}

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

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE
from esphome.components import uart
from esphome.const import CONF_ID, CONF_VOLTAGE, CONF_CURRENT

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

bl0906_ns = cg.esphome_ns.namespace("bl0906")
BL0906Component = bl0906_ns.class_(
    "BL0906Component", cg.PollingComponent, uart.UARTDevice
)

ResetEnergyAction = bl0906_ns.class_("ResetEnergyAction", automation.Action)
CalibrateZeroAction = bl0906_ns.class_("CalibrateZeroAction", automation.Action)

CONF_LOAD_RES = "load_res"
CONF_SAMPLE_RES = "sample_res"
CONF_SAMPLE_RATIO = "sample_ratio"
CONF_PGA_GAIN = "pga_gain"
CONF_VOLTAGE_REFERENCE = "voltage_reference"
CONF_CURRENT_REFERENCE = "current_reference"
CONF_POWER_REFERENCE = "power_reference"
CONF_ENERGY_REFERENCE = "energy_reference"
CONF_CFDIV = "cfdiv"
CONF_MAX_RETRIES = "max_retries"
CONF_IMMEDIATE_RETRIES = "immediate_retries"
CONF_RETRY_BACKOFF_BASE_MS = "retry_backoff_base_ms"
CONF_RETRY_BACKOFF_MULTIPLIER = "retry_backoff_multiplier"

PGA_GAINS = cv.one_of(1, 2, 8, 16, int=True)

VOLTAGE_FRONTEND_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LOAD_RES, default=100000.0): cv.positive_float,
        cv.Optional(CONF_SAMPLE_RES, default=100.0): cv.positive_float,
        cv.Optional(CONF_SAMPLE_RATIO, default=1.0): cv.positive_float,
        cv.Optional(CONF_PGA_GAIN, default=1): PGA_GAINS,
    }
)

CURRENT_FRONTEND_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SAMPLE_RES, default=0.001): cv.positive_float,
        cv.Optional(CONF_SAMPLE_RATIO, default=1.0): cv.positive_float,
        cv.Optional(CONF_PGA_GAIN, default=1): PGA_GAINS,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0906Component),
            cv.Optional(CONF_VOLTAGE, default={}): VOLTAGE_FRONTEND_SCHEMA,
            cv.Optional(CONF_CURRENT, default={}): CURRENT_FRONTEND_SCHEMA,
            cv.Optional(CONF_CFDIV, default=0x010): cv.int_range(min=0, max=0xFFF),
            cv.Optional(CONF_VOLTAGE_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_CURRENT_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_POWER_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_ENERGY_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_MAX_RETRIES, default=5): cv.int_range(min=1, max=20),
            cv.Optional(CONF_IMMEDIATE_RETRIES, default=3): cv.int_range(min=0, max=20),
            cv.Optional(CONF_RETRY_BACKOFF_BASE_MS, default=2): cv.int_range(min=0, max=1000),
            cv.Optional(CONF_RETRY_BACKOFF_MULTIPLIER, default=2): cv.int_range(min=1, max=8),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "bl0906", baud_rate=19200, require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    preference_key = config[CONF_ID].id or CORE.friendly_name.replace(" ", "_")
    cg.add(var.set_preferences_key(preference_key))
    cg.add(var.set_voltage_load_res(config[CONF_VOLTAGE][CONF_LOAD_RES]))
    cg.add(var.set_voltage_sample_res(config[CONF_VOLTAGE][CONF_SAMPLE_RES]))
    cg.add(var.set_voltage_sample_ratio(config[CONF_VOLTAGE][CONF_SAMPLE_RATIO]))
    cg.add(var.set_voltage_pga_gain(config[CONF_VOLTAGE][CONF_PGA_GAIN]))
    cg.add(var.set_current_sample_res(config[CONF_CURRENT][CONF_SAMPLE_RES]))
    cg.add(var.set_current_sample_ratio(config[CONF_CURRENT][CONF_SAMPLE_RATIO]))
    cg.add(var.set_current_pga_gain(config[CONF_CURRENT][CONF_PGA_GAIN]))
    cg.add(var.set_cfdiv(config[CONF_CFDIV]))
    cg.add(var.set_voltage_reference(config[CONF_VOLTAGE_REFERENCE]))
    cg.add(var.set_current_reference(config[CONF_CURRENT_REFERENCE]))
    cg.add(var.set_power_reference(config[CONF_POWER_REFERENCE]))
    cg.add(var.set_energy_reference(config[CONF_ENERGY_REFERENCE]))
    cg.add(var.set_max_read_attempts(config[CONF_MAX_RETRIES]))
    cg.add(var.set_immediate_read_attempts(config[CONF_IMMEDIATE_RETRIES]))
    cg.add(var.set_retry_backoff_base_ms(config[CONF_RETRY_BACKOFF_BASE_MS]))
    cg.add(var.set_retry_backoff_multiplier(config[CONF_RETRY_BACKOFF_MULTIPLIER]))


RESET_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(BL0906Component),
    }
)


@automation.register_action(
    "bl0906.reset_energy", ResetEnergyAction, RESET_ACTION_SCHEMA
)
async def bl0906_reset_energy_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


CALIBRATE_ZERO_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(BL0906Component),
    }
)


@automation.register_action(
    "bl0906.calibrate_zero", CalibrateZeroAction, CALIBRATE_ZERO_ACTION_SCHEMA
)
async def bl0906_calibrate_zero_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

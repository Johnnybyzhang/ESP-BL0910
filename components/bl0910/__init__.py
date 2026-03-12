import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_MODE

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

bl0910_ns = cg.esphome_ns.namespace("bl0910")
BL0910Component = bl0910_ns.class_(
    "BL0910Component", cg.PollingComponent, spi.SPIDevice
)

LineFrequency = bl0910_ns.enum("LineFrequency")
LINE_FREQUENCIES = {
    "50HZ": LineFrequency.LINE_FREQUENCY_50HZ,
    "60HZ": LineFrequency.LINE_FREQUENCY_60HZ,
}

MeasurementMode = bl0910_ns.enum("MeasurementMode")
MEASUREMENT_MODES = {
    "1U10I": MeasurementMode.MODE_1U10I,
    "5U5I": MeasurementMode.MODE_5U5I,
    "3U6I": MeasurementMode.MODE_3U6I,
}

ResetAction = bl0910_ns.class_("ResetAction", automation.Action)

CONF_RESET_PIN = "reset_pin"
CONF_IRQ_PIN = "irq_pin"
CONF_LINE_FREQUENCY = "line_frequency"
CONF_VOLTAGE_REFERENCE = "voltage_reference"
CONF_CURRENT_REFERENCE = "current_reference"
CONF_POWER_REFERENCE = "power_reference"
CONF_ENERGY_REFERENCE = "energy_reference"
CONF_VOLTAGE = "voltage"
CONF_CURRENT = "current"
CONF_LOAD_RES = "load_res"
CONF_SAMPLE_RES = "sample_res"
CONF_SAMPLE_RATIO = "sample_ratio"
CONF_PGA_GAIN = "pga_gain"
CONF_CFDIV = "cfdiv"

PGA_GAINS = cv.one_of(1, 2, 8, 16, int=True)

VOLTAGE_FRONTEND_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LOAD_RES, default=2000000.0): cv.positive_float,
        cv.Optional(CONF_SAMPLE_RES, default=510.0): cv.positive_float,
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
            cv.GenerateID(): cv.declare_id(BL0910Component),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MODE, default="1U10I"): cv.enum(
                MEASUREMENT_MODES, upper=True
            ),
            cv.Optional(CONF_LINE_FREQUENCY, default="50HZ"): cv.enum(
                LINE_FREQUENCIES, upper=True
            ),
            cv.Optional(CONF_VOLTAGE, default={}): VOLTAGE_FRONTEND_SCHEMA,
            cv.Optional(CONF_CURRENT, default={}): CURRENT_FRONTEND_SCHEMA,
            cv.Optional(CONF_CFDIV, default=0x010): cv.int_range(min=0, max=0xFFF),
            cv.Optional(CONF_VOLTAGE_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_CURRENT_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_POWER_REFERENCE, default=1.0): cv.float_,
            cv.Optional(CONF_ENERGY_REFERENCE, default=1.0): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))

    if CONF_IRQ_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(pin))

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_line_frequency(config[CONF_LINE_FREQUENCY]))
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


RESET_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(BL0910Component),
    }
)


@automation.register_action("bl0910.reset", ResetAction, RESET_ACTION_SCHEMA)
async def bl0910_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

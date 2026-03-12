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

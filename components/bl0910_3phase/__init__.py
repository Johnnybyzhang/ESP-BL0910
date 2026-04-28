import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

from ..bl0910 import BL0910Component

DEPENDENCIES = ["bl0910"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = False

bl0910_3phase_ns = cg.esphome_ns.namespace("bl0910_3phase")
BL09103PhaseComponent = bl0910_3phase_ns.class_(
    "BL09103PhaseComponent", cg.PollingComponent
)

LineFrequency = bl0910_3phase_ns.enum("LineFrequency")
LINE_FREQUENCIES = {
    "50HZ": LineFrequency.LINE_FREQUENCY_50HZ,
    "60HZ": LineFrequency.LINE_FREQUENCY_60HZ,
}

CONF_PHASE_A = "phase_a"
CONF_PHASE_B = "phase_b"
CONF_PHASE_C = "phase_c"
CONF_RESET_PIN = "reset_pin"
CONF_IRQ1_A_PIN = "irq1_a_pin"
CONF_IRQ1_B_PIN = "irq1_b_pin"
CONF_IRQ1_C_PIN = "irq1_c_pin"
CONF_IRQ2_A_PIN = "irq2_a_pin"
CONF_IRQ2_B_PIN = "irq2_b_pin"
CONF_IRQ2_C_PIN = "irq2_c_pin"
CONF_LINE_FREQUENCY = "line_frequency"


def validate_reset_pin(value):
    """Accept a single GPIO pin or a list of exactly 3 GPIO pins."""
    if isinstance(value, list):
        if len(value) != 3:
            raise cv.Invalid(
                "reset_pin list must have exactly 3 entries (one per phase A/B/C)"
            )
        return {"pins": [pins.gpio_output_pin_schema(v) for v in value]}
    return {"pins": [pins.gpio_output_pin_schema(value)]}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL09103PhaseComponent),
            cv.Required(CONF_PHASE_A): cv.use_id(BL0910Component),
            cv.Required(CONF_PHASE_B): cv.use_id(BL0910Component),
            cv.Required(CONF_PHASE_C): cv.use_id(BL0910Component),
            cv.Optional(CONF_RESET_PIN): validate_reset_pin,
            cv.Optional(CONF_IRQ1_A_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IRQ1_B_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IRQ1_C_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IRQ2_A_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IRQ2_B_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IRQ2_C_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_LINE_FREQUENCY, default="50HZ"): cv.enum(
                LINE_FREQUENCIES, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    phase_a = await cg.get_variable(config[CONF_PHASE_A])
    cg.add(var.set_phase_a(phase_a))
    phase_b = await cg.get_variable(config[CONF_PHASE_B])
    cg.add(var.set_phase_b(phase_b))
    phase_c = await cg.get_variable(config[CONF_PHASE_C])
    cg.add(var.set_phase_c(phase_c))

    if CONF_RESET_PIN in config:
        pin_configs = config[CONF_RESET_PIN]["pins"]
        if len(pin_configs) == 1:
            pin = await cg.gpio_pin_expression(pin_configs[0])
            cg.add(var.set_shared_reset_pin(pin))
        else:
            for i, pin_conf in enumerate(pin_configs):
                pin = await cg.gpio_pin_expression(pin_conf)
                cg.add(var.set_reset_pin(i, pin))

    for conf_key, setter in [
        (CONF_IRQ1_A_PIN, "set_irq1_a_pin"),
        (CONF_IRQ1_B_PIN, "set_irq1_b_pin"),
        (CONF_IRQ1_C_PIN, "set_irq1_c_pin"),
        (CONF_IRQ2_A_PIN, "set_irq2_a_pin"),
        (CONF_IRQ2_B_PIN, "set_irq2_b_pin"),
        (CONF_IRQ2_C_PIN, "set_irq2_c_pin"),
    ]:
        if conf_key in config:
            pin = await cg.gpio_pin_expression(config[conf_key])
            cg.add(getattr(var, setter)(pin))

    cg.add(var.set_line_frequency(config[CONF_LINE_FREQUENCY]))

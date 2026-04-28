import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import BL09103PhaseComponent

CONF_BL0910_3PHASE_ID = "bl0910_3phase_id"
CONF_PHASE_SEQUENCE = "phase_sequence"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BL0910_3PHASE_ID): cv.use_id(BL09103PhaseComponent),
        cv.Optional(CONF_PHASE_SEQUENCE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BL0910_3PHASE_ID])

    if CONF_PHASE_SEQUENCE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PHASE_SEQUENCE])
        cg.add(hub.set_phase_sequence_sensor(sens))

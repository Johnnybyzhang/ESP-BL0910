import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_FREQUENCY,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)
from . import BL09103PhaseComponent

CONF_BL0910_3PHASE_ID = "bl0910_3phase_id"
CONF_TOTAL_ACTIVE_POWER = "total_active_power"
CONF_TOTAL_REACTIVE_POWER = "total_reactive_power"
CONF_TOTAL_APPARENT_POWER = "total_apparent_power"
CONF_SYSTEM_POWER_FACTOR = "system_power_factor"
CONF_PHASE_ANGLE_AB = "phase_angle_ab"
CONF_PHASE_ANGLE_BC = "phase_angle_bc"
CONF_PHASE_ANGLE_CA = "phase_angle_ca"
CONF_VOLTAGE_UNBALANCE = "voltage_unbalance"
CONF_LINE_VOLTAGE_AB = "line_voltage_ab"
CONF_LINE_VOLTAGE_BC = "line_voltage_bc"
CONF_LINE_VOLTAGE_CA = "line_voltage_ca"

UNIT_DEGREES = "°"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BL0910_3PHASE_ID): cv.use_id(BL09103PhaseComponent),
        cv.Optional(CONF_TOTAL_ACTIVE_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TOTAL_REACTIVE_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TOTAL_APPARENT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SYSTEM_POWER_FACTOR): sensor.sensor_schema(
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER_FACTOR,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PHASE_ANGLE_AB): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PHASE_ANGLE_BC): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PHASE_ANGLE_CA): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VOLTAGE_UNBALANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LINE_VOLTAGE_AB): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LINE_VOLTAGE_BC): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LINE_VOLTAGE_CA): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

SENSOR_MAP = {
    CONF_TOTAL_ACTIVE_POWER: "set_total_active_power_sensor",
    CONF_TOTAL_REACTIVE_POWER: "set_total_reactive_power_sensor",
    CONF_TOTAL_APPARENT_POWER: "set_total_apparent_power_sensor",
    CONF_SYSTEM_POWER_FACTOR: "set_system_power_factor_sensor",
    CONF_FREQUENCY: "set_frequency_sensor",
    CONF_PHASE_ANGLE_AB: "set_phase_angle_ab_sensor",
    CONF_PHASE_ANGLE_BC: "set_phase_angle_bc_sensor",
    CONF_PHASE_ANGLE_CA: "set_phase_angle_ca_sensor",
    CONF_VOLTAGE_UNBALANCE: "set_voltage_unbalance_sensor",
    CONF_LINE_VOLTAGE_AB: "set_line_voltage_ab_sensor",
    CONF_LINE_VOLTAGE_BC: "set_line_voltage_bc_sensor",
    CONF_LINE_VOLTAGE_CA: "set_line_voltage_ca_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BL0910_3PHASE_ID])

    for conf_key, setter_name in SENSOR_MAP.items():
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(getattr(hub, setter_name)(sens))

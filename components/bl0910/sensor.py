import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_FREQUENCY,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)
from . import BL0910Component

CONF_BL0910_ID = "bl0910_id"
CONF_TOTAL_POWER = "total_power"
CONF_TOTAL_ENERGY = "total_energy"

CHANNEL_KEYS = [f"channel_{i}" for i in range(1, 11)]

CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER_FACTOR,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BL0910_ID): cv.use_id(BL0910Component),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TOTAL_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TOTAL_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        **{cv.Optional(ch_key): CHANNEL_SCHEMA for ch_key in CHANNEL_KEYS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BL0910_ID])

    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(hub.set_voltage_sensor(sens))

    if CONF_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(hub.set_frequency_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(hub.set_temperature_sensor(sens))

    if CONF_TOTAL_POWER in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_POWER])
        cg.add(hub.set_total_power_sensor(sens))

    if CONF_TOTAL_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_ENERGY])
        cg.add(hub.set_total_energy_sensor(sens))

    for i, ch_key in enumerate(CHANNEL_KEYS):
        if ch_key in config:
            ch_conf = config[ch_key]
            if CONF_CURRENT in ch_conf:
                sens = await sensor.new_sensor(ch_conf[CONF_CURRENT])
                cg.add(hub.set_current_sensor(i, sens))
            if CONF_POWER in ch_conf:
                sens = await sensor.new_sensor(ch_conf[CONF_POWER])
                cg.add(hub.set_power_sensor(i, sens))
            if CONF_ENERGY in ch_conf:
                sens = await sensor.new_sensor(ch_conf[CONF_ENERGY])
                cg.add(hub.set_energy_sensor(i, sens))
            if CONF_POWER_FACTOR in ch_conf:
                sens = await sensor.new_sensor(ch_conf[CONF_POWER_FACTOR])
                cg.add(hub.set_power_factor_sensor(i, sens))

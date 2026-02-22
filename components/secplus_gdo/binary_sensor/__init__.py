import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOBinarySensor = secplus_gdo_ns.class_(
    "GDOBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_TYPE = "type"
TYPE_OPTIONS = [
    "motion",
    "obstruction",
    "motor",
    "button",
    "sync",
    "wireless_remote",
]
REGISTER_METHODS = {
    "motion": "register_motion_sensor",
    "obstruction": "register_obstruction_sensor",
    "motor": "register_motor_sensor",
    "button": "register_button_sensor",
    "sync": "register_sync_sensor",
    "wireless_remote": "register_wireless_remote_sensor",
}

_TYPE_DEFAULTS = {
    "motion": {CONF_DEVICE_CLASS: "motion"},
    "obstruction": {CONF_DEVICE_CLASS: "problem"},
    "motor": {CONF_DEVICE_CLASS: "running"},
    "sync": {CONF_DEVICE_CLASS: "connectivity"},
    "wireless_remote": {
        CONF_ICON: "mdi:remote",
        CONF_DISABLED_BY_DEFAULT: True,
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    },
    "button": {
        CONF_DISABLED_BY_DEFAULT: True,
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    },
}


def _apply_type_defaults(config):
    config = dict(config)
    defaults = _TYPE_DEFAULTS.get(config[CONF_TYPE], {})
    for key, value in defaults.items():
        config.setdefault(key, value)
    return config


CONFIG_SCHEMA = cv.All(
    _apply_type_defaults,
    binary_sensor.binary_sensor_schema(GDOBinarySensor)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.one_of(*TYPE_OPTIONS, lower=True),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(getattr(parent, REGISTER_METHODS[config[CONF_TYPE]])(var))

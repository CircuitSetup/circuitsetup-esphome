import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOTextSensor = secplus_gdo_ns.class_(
    "GDOTextSensor", text_sensor.TextSensor, cg.Component
)

CONF_TYPE = "type"
TYPE_OPTIONS = [
    "battery",
    "protocol",
    "gdolib_version",
    "opener_id",
    "last_error",
    "last_event",
]
REGISTER_METHODS = {
    "battery": "register_battery_text_sensor",
    "protocol": "register_protocol_text_sensor",
    "gdolib_version": "register_gdolib_version_text_sensor",
    "opener_id": "register_opener_id_text_sensor",
    "last_error": "register_last_error_text_sensor",
    "last_event": "register_last_event_text_sensor",
}

_TYPE_DEFAULTS = {
    "battery": {CONF_ICON: "mdi:battery"},
    "protocol": {
        CONF_ICON: "mdi:protocol",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    },
    "gdolib_version": {
        CONF_ICON: "mdi:source-branch",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "opener_id": {
        CONF_ICON: "mdi:identifier",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "last_error": {
        CONF_ICON: "mdi:alert-circle-outline",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "last_event": {
        CONF_ICON: "mdi:timeline-text-outline",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
}


def _apply_type_defaults(config):
    config = dict(config)
    defaults = _TYPE_DEFAULTS.get(config[CONF_TYPE], {})
    for key, value in defaults.items():
        config.setdefault(key, value)
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(GDOTextSensor)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.one_of(*TYPE_OPTIONS, lower=True),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA),
    _apply_type_defaults,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(getattr(parent, REGISTER_METHODS[config[CONF_TYPE]])(var))

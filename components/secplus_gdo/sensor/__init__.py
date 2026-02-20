import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOStat = secplus_gdo_ns.class_("GDOStat", sensor.Sensor, cg.Component)

CONF_TYPE = "type"
TYPE_OPTIONS = [
    "openings",
    "paired_devices_total",
    "paired_devices_remotes",
    "paired_devices_keypads",
    "paired_devices_wall_controls",
    "paired_devices_accessories",
]
REGISTER_METHODS = {
    "openings": "register_openings_sensor",
    "paired_devices_total": "register_paired_total_sensor",
    "paired_devices_remotes": "register_paired_remotes_sensor",
    "paired_devices_keypads": "register_paired_keypads_sensor",
    "paired_devices_wall_controls": "register_paired_wall_controls_sensor",
    "paired_devices_accessories": "register_paired_accessories_sensor",
}

_TYPE_DEFAULTS = {
    "openings": {
        CONF_UNIT_OF_MEASUREMENT: "openings",
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_ICON: "mdi:counter",
    },
    "paired_devices_total": {
        CONF_ICON: "mdi:account-multiple",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "paired_devices_remotes": {
        CONF_ICON: "mdi:remote",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "paired_devices_keypads": {
        CONF_ICON: "mdi:dialpad",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "paired_devices_wall_controls": {
        CONF_ICON: "mdi:gesture-tap-button",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "paired_devices_accessories": {
        CONF_ICON: "mdi:puzzle",
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
    sensor.sensor_schema(GDOStat)
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
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(getattr(parent, REGISTER_METHODS[config[CONF_TYPE]])(var))

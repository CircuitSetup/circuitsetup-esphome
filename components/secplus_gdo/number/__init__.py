import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_MODE,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDONumber = secplus_gdo_ns.class_("GDONumber", number.Number, cg.Component)
NumberType = secplus_gdo_ns.enum("NumberType")

CONF_TYPE = "type"
TYPE_OPTIONS = [
    "open_duration",
    "close_duration",
    "client_id",
    "rolling_code",
]

TYPE_ENUMS = {
    "open_duration": NumberType.OPEN_DURATION,
    "close_duration": NumberType.CLOSE_DURATION,
    "client_id": NumberType.CLIENT_ID,
    "rolling_code": NumberType.ROLLING_CODE,
}

REGISTER_METHODS = {
    "open_duration": "register_open_duration",
    "close_duration": "register_close_duration",
    "client_id": "register_client_id",
    "rolling_code": "register_rolling_code",
}

_TYPE_DEFAULTS = {
    "open_duration": {
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "close_duration": {
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "client_id": {
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
        CONF_MODE: "box",
    },
    "rolling_code": {
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
        CONF_MODE: "box",
    },
}


def _apply_type_defaults(config):
    config = dict(config)
    defaults = _TYPE_DEFAULTS.get(config[CONF_TYPE], {})
    for key, value in defaults.items():
        config.setdefault(key, value)
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(GDONumber)
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

    if config[CONF_TYPE] in ("open_duration", "close_duration"):
        await number.register_number(var, config, min_value=0, max_value=0xFFFF, step=1)
    else:
        await number.register_number(var, config, min_value=0, max_value=0xFFFFFFFF, step=1)

    await cg.register_component(var, config)

    cg.add(var.set_type(TYPE_ENUMS[config[CONF_TYPE]]))

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(getattr(parent, REGISTER_METHODS[config[CONF_TYPE]])(var))

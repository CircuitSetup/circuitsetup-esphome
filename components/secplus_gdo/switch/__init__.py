import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOSwitch = secplus_gdo_ns.class_("GDOSwitch", switch.Switch, cg.Component)
SwitchType = secplus_gdo_ns.enum("SwitchType")

CONF_TYPE = "type"
TYPE_OPTIONS = ["learn", "toggle_only"]
TYPE_ENUMS = {
    "learn": SwitchType.LEARN,
    "toggle_only": SwitchType.TOGGLE_ONLY,
}
REGISTER_METHODS = {
    "learn": "register_learn",
    "toggle_only": "register_toggle_only",
}

_TYPE_DEFAULTS = {
    "learn": {
        CONF_ICON: "mdi:school",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
    },
    "toggle_only": {
        CONF_ICON: "mdi:swap-horizontal",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
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
    switch.switch_schema(GDOSwitch)
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
    await switch.register_switch(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_type(TYPE_ENUMS[config[CONF_TYPE]]))

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(getattr(parent, REGISTER_METHODS[config[CONF_TYPE]])(var))

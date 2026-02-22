import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOCommandButton = secplus_gdo_ns.class_(
    "GDOCommandButton", button.Button, cg.Component
)
CommandButtonType = secplus_gdo_ns.enum("CommandButtonType", is_class=True)

CONF_TYPE = "type"
TYPE_OPTIONS = [
    "sync",
    "resync",
    "restart_comms",
    "reset_travel_timings",
    "door_open",
    "door_close",
    "door_stop",
    "door_toggle",
    "light_toggle",
    "lock_toggle",
]
TYPE_ENUMS = {
    "sync": CommandButtonType.SYNC,
    "resync": CommandButtonType.RESYNC,
    "restart_comms": CommandButtonType.RESTART_COMMS,
    "reset_travel_timings": CommandButtonType.RESET_TRAVEL_TIMINGS,
    "door_open": CommandButtonType.DOOR_OPEN,
    "door_close": CommandButtonType.DOOR_CLOSE,
    "door_stop": CommandButtonType.DOOR_STOP,
    "door_toggle": CommandButtonType.DOOR_TOGGLE,
    "light_toggle": CommandButtonType.LIGHT_TOGGLE,
    "lock_toggle": CommandButtonType.LOCK_TOGGLE,
}

_TYPE_DEFAULTS = {
    "sync": {
        CONF_NAME: "Sync opener",
        CONF_ICON: "mdi:sync",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
    },
    "resync": {
        CONF_NAME: "Resync opener",
        CONF_ICON: "mdi:sync-alert",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
    },
    "restart_comms": {
        CONF_NAME: "Restart GDO comms",
        CONF_ICON: "mdi:restart",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "reset_travel_timings": {
        CONF_NAME: "Reset travel timings",
        CONF_ICON: "mdi:timer-refresh",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "door_open": {
        CONF_NAME: "Door open",
        CONF_ICON: "mdi:garage-open",
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "door_close": {
        CONF_NAME: "Door close",
        CONF_ICON: "mdi:garage",
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "door_stop": {
        CONF_NAME: "Door stop",
        CONF_ICON: "mdi:garage-alert",
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "door_toggle": {
        CONF_NAME: "Door toggle",
        CONF_ICON: "mdi:garage-variant",
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "light_toggle": {
        CONF_NAME: "Light toggle",
        CONF_ICON: "mdi:lightbulb-on-outline",
        CONF_DISABLED_BY_DEFAULT: True,
    },
    "lock_toggle": {
        CONF_NAME: "Lock toggle",
        CONF_ICON: "mdi:lock-reset",
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
    _apply_type_defaults,
    button.button_schema(GDOCommandButton)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.one_of(*TYPE_OPTIONS, lower=True),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA),
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(TYPE_ENUMS[config[CONF_TYPE]]))

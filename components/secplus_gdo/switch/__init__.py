import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns, validate_cpp_symbol_id

DEPENDENCIES = ["secplus_gdo"]

GDOSwitch = secplus_gdo_ns.class_("GDOSwitch", switch.Switch, cg.Component)

CONF_TYPE = "type"
TYPES = {
    "learn": 0,
    "toggle_only": 1,
}

CONFIG_SCHEMA = cv.All(
    switch.switch_schema(GDOSwitch)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA),
    validate_cpp_symbol_id,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(var.set_type(TYPES[config[CONF_TYPE]]))
    cg.add(parent.register_switch(var))

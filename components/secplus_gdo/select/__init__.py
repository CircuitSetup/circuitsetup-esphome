import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_INITIAL_OPTION

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO, secplus_gdo_ns, validate_cpp_symbol_id

DEPENDENCIES = ["secplus_gdo"]

GDOSelect = secplus_gdo_ns.class_("GDOSelect", select.Select, cg.Component)

CONF_PROTOCOL_SELECT_OPTIONS = ["auto", "security+1.0", "security+2.0", "security+1.0 with smart panel"]

CONFIG_SCHEMA = cv.All(
    select.select_schema(GDOSelect)
    .extend(
        {
            cv.Required(CONF_SECPLUS_GDO_ID): cv.use_id(SECPLUS_GDO),
            cv.Optional(CONF_INITIAL_OPTION, "auto"): cv.one_of(*CONF_PROTOCOL_SELECT_OPTIONS, lower=True),
        }
    ),
    validate_cpp_symbol_id,
)


async def to_code(config):
    select_var = await select.new_select(config, options=CONF_PROTOCOL_SELECT_OPTIONS)
    await cg.register_component(select_var, config)
    cg.add(select_var.set_initial_option(config[CONF_INITIAL_OPTION]))
    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(parent.register_protocol_select(select_var))

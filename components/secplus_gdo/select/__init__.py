import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_INITIAL_OPTION, ENTITY_CATEGORY_CONFIG

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOSelect = secplus_gdo_ns.class_("GDOSelect", select.Select, cg.Component)

PROTOCOL_OPTIONS = [
    "auto",
    "security+1.0",
    "security+2.0",
    "security+1.0 with smart panel",
]

CONFIG_SCHEMA = (
    select.select_schema(
        GDOSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:cog-sync",
    )
    .extend(
        {
            cv.Optional(CONF_INITIAL_OPTION, default="auto"): cv.one_of(
                *PROTOCOL_OPTIONS, lower=True
            ),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA)
)


async def to_code(config):
    var = await select.new_select(config, options=PROTOCOL_OPTIONS)
    await cg.register_component(var, config)

    cg.add(var.set_initial_option(config[CONF_INITIAL_OPTION]))

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(parent.register_protocol_select(var))

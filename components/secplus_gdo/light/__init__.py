import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOLight = secplus_gdo_ns.class_("GDOLight", light.LightOutput, cg.Component)

CONFIG_SCHEMA = (
    light.light_schema(
        GDOLight,
        light.LightType.BINARY,
        icon="mdi:lightbulb",
        default_restore_mode="ALWAYS_OFF",
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(parent.register_light(var))

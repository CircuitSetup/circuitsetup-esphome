import esphome.codegen as cg
from esphome.components import lock

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDOLock = secplus_gdo_ns.class_("GDOLock", lock.Lock, cg.Component)

CONFIG_SCHEMA = (
    lock.lock_schema(
        GDOLock,
        icon="mdi:lock",
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA)
)


async def to_code(config):
    var = await lock.new_lock(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(parent.register_lock(var))

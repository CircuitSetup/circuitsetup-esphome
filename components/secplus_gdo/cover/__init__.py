import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.const import CONF_TRIGGER_ID, DEVICE_CLASS_GARAGE

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns

DEPENDENCIES = ["secplus_gdo"]

GDODoor = secplus_gdo_ns.class_("GDODoor", cover.Cover, cg.Component)

CoverClosingStartTrigger = secplus_gdo_ns.class_(
    "CoverClosingStartTrigger", automation.Trigger.template()
)

CoverClosingEndTrigger = secplus_gdo_ns.class_(
    "CoverClosingEndTrigger", automation.Trigger.template()
)

CONF_PRE_CLOSE_WARNING_DURATION = "pre_close_warning_duration"
CONF_PRE_CLOSE_WARNING_START = "pre_close_warning_start"
CONF_PRE_CLOSE_WARNING_END = "pre_close_warning_end"

CONFIG_SCHEMA = (
    cover.cover_schema(GDODoor, device_class=DEVICE_CLASS_GARAGE)
    .extend(
        {
            cv.Optional(
                CONF_PRE_CLOSE_WARNING_DURATION, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PRE_CLOSE_WARNING_START): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CoverClosingStartTrigger)}
            ),
            cv.Optional(CONF_PRE_CLOSE_WARNING_END): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CoverClosingEndTrigger)}
            ),
        }
    )
    .extend(SECPLUS_GDO_CONFIG_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(parent.register_door(var))

    cg.add(var.set_pre_close_warning_duration(config[CONF_PRE_CLOSE_WARNING_DURATION]))

    for conf in config.get(CONF_PRE_CLOSE_WARNING_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        cg.add(var.register_door_closing_warn_start_trigger(trigger))

    for conf in config.get(CONF_PRE_CLOSE_WARNING_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        cg.add(var.register_door_closing_warn_end_trigger(trigger))

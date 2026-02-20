"""
secplus_gdo root component configuration.
"""

import esphome.automation as automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import wifi
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["preferences"]
MULTI_CONF = True

CONF_OUTPUT_GDO = "output_gdo_pin"
CONF_INPUT_GDO = "input_gdo_pin"
CONF_UART_TX_PIN = "uart_tx_pin"
CONF_UART_RX_PIN = "uart_rx_pin"
CONF_OBSTRUCTION_FROM_STATUS = "obstruction_from_status"
CONF_INVERT_UART = "invert_uart"
CONF_OBSTRUCTION_INPUT_PIN = "obstruction_input_pin"
CONF_MIN_COMMAND_INTERVAL = "min_command_interval"
CONF_SYNC_RETRY_INTERVAL = "sync_retry_interval"
CONF_AUTO_START = "auto_start"
CONF_SECPLUS_GDO_ID = "secplus_gdo_id"

secplus_gdo_ns = cg.esphome_ns.namespace("secplus_gdo")
SECPLUS_GDO = secplus_gdo_ns.class_("GDOComponent", cg.Component)

SyncAction = secplus_gdo_ns.class_("SyncAction", automation.Action)
ResyncAction = secplus_gdo_ns.class_("ResyncAction", automation.Action)
RestartCommsAction = secplus_gdo_ns.class_("RestartCommsAction", automation.Action)
ResetTravelTimingsAction = secplus_gdo_ns.class_("ResetTravelTimingsAction", automation.Action)
DoorOpenAction = secplus_gdo_ns.class_("DoorOpenAction", automation.Action)
DoorCloseAction = secplus_gdo_ns.class_("DoorCloseAction", automation.Action)
DoorStopAction = secplus_gdo_ns.class_("DoorStopAction", automation.Action)
DoorToggleAction = secplus_gdo_ns.class_("DoorToggleAction", automation.Action)
LightToggleAction = secplus_gdo_ns.class_("LightToggleAction", automation.Action)
LockToggleAction = secplus_gdo_ns.class_("LockToggleAction", automation.Action)


def _normalize_and_validate_pins(config):
    tx_pin = config.get(CONF_UART_TX_PIN, config.get(CONF_OUTPUT_GDO, 1))
    rx_pin = config.get(CONF_UART_RX_PIN, config.get(CONF_INPUT_GDO, 2))

    if CONF_UART_TX_PIN in config and CONF_OUTPUT_GDO in config:
        raise cv.Invalid(
            f"Only one of '{CONF_UART_TX_PIN}' or '{CONF_OUTPUT_GDO}' may be set."
        )
    if CONF_UART_RX_PIN in config and CONF_INPUT_GDO in config:
        raise cv.Invalid(
            f"Only one of '{CONF_UART_RX_PIN}' or '{CONF_INPUT_GDO}' may be set."
        )

    if tx_pin == rx_pin:
        raise cv.Invalid(
            f"'{CONF_OUTPUT_GDO}' and '{CONF_INPUT_GDO}' must use different GPIO pins."
        )

    config = dict(config)
    config[CONF_OUTPUT_GDO] = tx_pin
    config[CONF_INPUT_GDO] = rx_pin
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SECPLUS_GDO),
            cv.Optional(CONF_OUTPUT_GDO): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_INPUT_GDO): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_UART_TX_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_UART_RX_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_OBSTRUCTION_FROM_STATUS, default=True): cv.boolean,
            cv.Optional(CONF_INVERT_UART, default=True): cv.boolean,
            cv.Optional(CONF_OBSTRUCTION_INPUT_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_MIN_COMMAND_INTERVAL, default="50ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SYNC_RETRY_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_START, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _normalize_and_validate_pins,
)

SECPLUS_GDO_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SECPLUS_GDO_ID): cv.use_id(SECPLUS_GDO),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_uart_tx_pin(config[CONF_OUTPUT_GDO]))
    cg.add(var.set_uart_rx_pin(config[CONF_INPUT_GDO]))
    cg.add(var.set_obstruction_from_status(config[CONF_OBSTRUCTION_FROM_STATUS]))
    cg.add(var.set_invert_uart(config[CONF_INVERT_UART]))
    if CONF_OBSTRUCTION_INPUT_PIN in config:
        cg.add(var.set_obstruction_input_pin(config[CONF_OBSTRUCTION_INPUT_PIN]))

    cg.add(var.set_min_command_interval(config[CONF_MIN_COMMAND_INTERVAL].total_milliseconds))
    cg.add(var.set_sync_retry_interval(config[CONF_SYNC_RETRY_INTERVAL].total_milliseconds))
    cg.add(var.set_auto_start(config[CONF_AUTO_START]))

    if CORE.is_esp32:
        cg.add_build_flag("-Wl,--wrap=esp_panic_handler")

    if "wifi" in CORE.config:
        wifi.request_wifi_connect_state_listener()


ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SECPLUS_GDO),
    }
)


@automation.register_action("secplus_gdo.sync", SyncAction, ACTION_SCHEMA)
async def secplus_gdo_sync_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.resync", ResyncAction, ACTION_SCHEMA)
async def secplus_gdo_resync_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.restart_comms", RestartCommsAction, ACTION_SCHEMA)
async def secplus_gdo_restart_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "secplus_gdo.reset_travel_timings", ResetTravelTimingsAction, ACTION_SCHEMA
)
async def secplus_gdo_reset_timings_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.door_open", DoorOpenAction, ACTION_SCHEMA)
async def secplus_gdo_door_open_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.door_close", DoorCloseAction, ACTION_SCHEMA)
async def secplus_gdo_door_close_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.door_stop", DoorStopAction, ACTION_SCHEMA)
async def secplus_gdo_door_stop_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.door_toggle", DoorToggleAction, ACTION_SCHEMA)
async def secplus_gdo_door_toggle_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.light_toggle", LightToggleAction, ACTION_SCHEMA)
async def secplus_gdo_light_toggle_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("secplus_gdo.lock_toggle", LockToggleAction, ACTION_SCHEMA)
async def secplus_gdo_lock_toggle_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)

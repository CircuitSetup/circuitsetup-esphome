"""
/*
 * Copyright (C) 2024  Konnected Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 """

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_NUMBER
from esphome.core import CORE

DEPENDENCIES = ["preferences"]
MULTI_CONF = True

secplus_gdo_ns = cg.esphome_ns.namespace("secplus_gdo")
SECPLUS_GDO = secplus_gdo_ns.class_("GDOComponent", cg.Component)

CONF_OUTPUT_GDO = "output_gdo_pin"
CONF_INPUT_GDO = "input_gdo_pin"
CONF_SECPLUS_GDO_ID = "secplus_gdo_id"

GDO_RESERVED_IDS = frozenset(
    {
        "gdo_activate_learn",
        "gdo_clear_paired_devices",
        "gdo_deactivate_learn",
        "gdo_deinit",
        "gdo_door_close",
        "gdo_door_move_to_target",
        "gdo_door_open",
        "gdo_door_state_to_string",
        "gdo_door_stop",
        "gdo_door_toggle",
        "gdo_get_status",
        "gdo_init",
        "gdo_learn_state_to_string",
        "gdo_light_off",
        "gdo_light_on",
        "gdo_light_state_to_string",
        "gdo_light_toggle",
        "gdo_lock",
        "gdo_lock_state_to_string",
        "gdo_motion_state_to_string",
        "gdo_obstruction_state_to_string",
        "gdo_paired_device_type_to_string",
        "gdo_protocol_type_to_string",
        "gdo_set_client_id",
        "gdo_set_close_duration",
        "gdo_set_min_command_interval",
        "gdo_set_open_duration",
        "gdo_set_protocol",
        "gdo_set_rolling_code",
        "gdo_set_toggle_only",
        "gdo_start",
        "gdo_sync",
        "gdo_toggle_lock",
        "gdo_unlock",
        "gdo_motor_state_to_string",
        "gdo_button_state_to_string",
        "gdo_battery_state_to_string",
    }
)


def validate_cpp_symbol_id(config, key=CONF_ID):
    generated_id = config.get(key)
    if generated_id is None:
        return config

    generated_name = getattr(generated_id, "id", None)
    if generated_name in GDO_RESERVED_IDS:
        raise cv.Invalid(
            f"'{generated_name}' is reserved by gdolib and cannot be used as an ESPHome id. "
            "Choose a different id to avoid generated C++ symbol collisions."
        )

    return config


def validate_gdo_pins(config):
    if config[CONF_OUTPUT_GDO][CONF_NUMBER] == config[CONF_INPUT_GDO][CONF_NUMBER]:
        raise cv.Invalid("input_gdo_pin and output_gdo_pin must use different pins")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SECPLUS_GDO),
            cv.Required(CONF_OUTPUT_GDO): pins.gpio_output_pin_schema,
            cv.Required(CONF_INPUT_GDO): pins.gpio_input_pin_schema,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_gdo_pins,
    validate_cpp_symbol_id,
)

SECPLUS_GDO_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SECPLUS_GDO_ID): cv.use_id(SECPLUS_GDO),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.is_esp32 and CORE.target_framework == "esp-idf":
        # secplus_gdo needs to own the panic wrapper on ESP-IDF so the GDO TX pin can
        # be forced into a safe state before panic output proceeds.
        cg.add_build_unflag("-Wl,--wrap=esp_panic_handler")
        cg.add_build_unflag("-DUSE_ESP32_CRASH_HANDLER")
        cg.add_build_flag("-Wl,--wrap=esp_panic_handler")

    cg.add_define("GDO_UART_TX_PIN", config[CONF_OUTPUT_GDO][CONF_NUMBER])
    cg.add_define("GDO_UART_RX_PIN", config[CONF_INPUT_GDO][CONF_NUMBER])

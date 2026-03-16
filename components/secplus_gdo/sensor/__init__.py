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
from esphome.components import sensor
from esphome.const import CONF_ID

from .. import CONF_SECPLUS_GDO_ID, SECPLUS_GDO_CONFIG_SCHEMA, secplus_gdo_ns, validate_cpp_symbol_id

DEPENDENCIES = ["secplus_gdo"]

GDOStat = secplus_gdo_ns.class_("GDOStat", sensor.Sensor, cg.Component)

CONF_TYPE = "type"
TYPES = {
    "openings": 0,
    "paired_devices_total": 1,
    "paired_devices_remotes": 2,
    "paired_devices_keypads": 3,
    "paired_devices_wall_controls": 4,
    "paired_devices_accessories": 5,
}

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(GDOStat)
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
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_SECPLUS_GDO_ID])
    cg.add(var.set_type(TYPES[config[CONF_TYPE]]))
    cg.add(parent.register_sensor(var))

/*
 * Copyright (C) 2024  Konnected Inc.
 * Copyright (C) 2026  CircuitSetup
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

#pragma once

#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

enum class GDOStatType : uint8_t {
    OPENINGS = 0,
    PAIRED_DEVICES_TOTAL,
    PAIRED_DEVICES_REMOTES,
    PAIRED_DEVICES_KEYPADS,
    PAIRED_DEVICES_WALL_CONTROLS,
    PAIRED_DEVICES_ACCESSORIES,
};

class GDOStat : public sensor::Sensor, public Component {
public:
    void dump_config() override { ESP_LOGCONFIG(TAG, "GDO sensor type: %s", this->type_to_string_()); }
    void set_type(uint8_t type) { this->type_ = static_cast<GDOStatType>(type); }
    GDOStatType get_type() const { return this->type_; }
    void update_state(uint32_t value) { this->publish_state(value); }

protected:
    const char *type_to_string_() const {
        switch (this->type_) {
        case GDOStatType::OPENINGS:
            return "openings";
        case GDOStatType::PAIRED_DEVICES_TOTAL:
            return "paired_devices_total";
        case GDOStatType::PAIRED_DEVICES_REMOTES:
            return "paired_devices_remotes";
        case GDOStatType::PAIRED_DEVICES_KEYPADS:
            return "paired_devices_keypads";
        case GDOStatType::PAIRED_DEVICES_WALL_CONTROLS:
            return "paired_devices_wall_controls";
        case GDOStatType::PAIRED_DEVICES_ACCESSORIES:
            return "paired_devices_accessories";
        default:
            return "unknown";
        }
    }

    GDOStatType type_{GDOStatType::OPENINGS};
    static constexpr const char *TAG = "gdo.sensor";
};

} // namespace secplus_gdo
} // namespace esphome

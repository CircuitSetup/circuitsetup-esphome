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

#pragma once

#include <cstdint>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

enum class GDOBinarySensorType : uint8_t {
    MOTION = 0,
    OBSTRUCTION,
    MOTOR,
    BUTTON,
    SYNC,
    WIRELESS_REMOTE,
};

class GDOBinarySensor : public binary_sensor::BinarySensor, public Component {
public:
    void dump_config() override { ESP_LOGCONFIG(TAG, "GDO binary sensor type: %s", this->type_to_string_()); }
    void set_type(uint8_t type) { this->type_ = static_cast<GDOBinarySensorType>(type); }
    GDOBinarySensorType get_type() const { return this->type_; }
    void publish(bool state) { this->publish_state(state); }

protected:
    const char *type_to_string_() const {
        switch (this->type_) {
        case GDOBinarySensorType::MOTION:
            return "motion";
        case GDOBinarySensorType::OBSTRUCTION:
            return "obstruction";
        case GDOBinarySensorType::MOTOR:
            return "motor";
        case GDOBinarySensorType::BUTTON:
            return "button";
        case GDOBinarySensorType::SYNC:
            return "sync";
        case GDOBinarySensorType::WIRELESS_REMOTE:
            return "wireless_remote";
        default:
            return "unknown";
        }
    }

    GDOBinarySensorType type_{GDOBinarySensorType::MOTION};
    static constexpr const char *TAG = "gdo.binary_sensor";
};

} // namespace secplus_gdo
} // namespace esphome

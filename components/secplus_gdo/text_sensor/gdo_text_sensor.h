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
#include <string>

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

enum class GDOTextSensorType : uint8_t {
    BATTERY = 0,
};

class GDOTextSensor : public text_sensor::TextSensor, public Component {
public:
    void dump_config() override { ESP_LOGCONFIG(TAG, "GDO text sensor type: %s", this->type_to_string_()); }
    void set_type(uint8_t type) { this->type_ = static_cast<GDOTextSensorType>(type); }
    GDOTextSensorType get_type() const { return this->type_; }
    void update_state(const std::string &value) { this->publish_state(value); }

protected:
    const char *type_to_string_() const {
        switch (this->type_) {
        case GDOTextSensorType::BATTERY:
            return "battery";
        default:
            return "unknown";
        }
    }

    GDOTextSensorType type_{GDOTextSensorType::BATTERY};
    static constexpr const char *TAG = "gdo.text_sensor";
};

} // namespace secplus_gdo
} // namespace esphome

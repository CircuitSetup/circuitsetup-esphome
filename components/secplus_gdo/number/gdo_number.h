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

#include <utility>

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "gdo.h"

namespace esphome {
namespace secplus_gdo {

enum class GDONumberType : uint8_t {
    OPEN_DURATION = 0,
    CLOSE_DURATION,
    CLIENT_ID,
    ROLLING_CODE,
};

class GDONumber : public number::Number, public Component {
public:
    void dump_config() override { ESP_LOGCONFIG(TAG, "GDO number type: %s", this->type_to_string_()); }
    void set_type(uint8_t type) { this->type_ = static_cast<GDONumberType>(type); }
    GDONumberType get_type() const { return this->type_; }

    void setup() override {
        double value = 0;
        this->pref_ = this->make_entity_preference<double>();
        if (this->pref_.load(&value)) {
            this->apply_value_(value);
            return;
        }

        // Older firmware stored GDONumber values as float. Try that once so
        // existing rolling-code preferences are not silently discarded.
        float legacy_value = 0.0f;
        auto legacy_pref = this->make_entity_preference<float>();
        if (legacy_pref.load(&legacy_value)) {
            this->apply_value_(static_cast<double>(legacy_value));
        }
    }

    void update_state(double value) {
        if (this->has_state_ && value == this->value_) {
            return;
        }

        this->value_ = value;
        this->state = static_cast<float>(value);
        this->publish_state(static_cast<float>(value));
        this->pref_.save(&value);
        this->has_state_ = true;
    }

    void control(float value) override {
        this->apply_value_(static_cast<double>(value));
    }

    void set_control_function(std::function<esp_err_t(double)> f) { this->f_control = std::move(f); }

protected:
    void apply_value_(double value) {
        if (this->has_state_ && value == this->value_) {
            return;
        }

        if (!this->f_control) {
            ESP_LOGW(TAG, "No control function configured for %s", this->type_to_string_());
            return;
        }

        const auto err = this->f_control(value);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set %s: %s", this->type_to_string_(), esp_err_to_name(err));
            return;
        }

        this->update_state(value);
    }

    const char *type_to_string_() const {
        switch (this->type_) {
        case GDONumberType::OPEN_DURATION:
            return "open_duration";
        case GDONumberType::CLOSE_DURATION:
            return "close_duration";
        case GDONumberType::CLIENT_ID:
            return "client_id";
        case GDONumberType::ROLLING_CODE:
            return "rolling_code";
        default:
            return "unknown";
        }
    }

    GDONumberType type_{GDONumberType::OPEN_DURATION};
    ESPPreferenceObject pref_;
    std::function<esp_err_t(double)> f_control{nullptr};
    double value_{0};
    bool has_state_{false};
    static constexpr const char *TAG = "gdo.number";
};

} // namespace secplus_gdo
} // namespace esphome

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

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "../gdolib/gdo.h"

namespace esphome {
namespace secplus_gdo {

enum class NumberType {
  OPEN_DURATION,
  CLOSE_DURATION,
  CLIENT_ID,
  ROLLING_CODE,
};

class GDONumber : public number::Number, public Component {
 public:
  void dump_config() override {}

  void setup() override {
    float value = 0.0f;
    this->pref_ = this->make_entity_preference<float>();
    if (!this->pref_.load(&value)) {
      value = 0.0f;
      this->has_state_value_ = false;
    } else {
      this->has_state_value_ = true;
    }

    this->state = value;
    this->publish_state(value);
  }

  void update_state(float value) {
    if (this->has_state_value_ && value == this->state) {
      return;
    }
    this->state = value;
    this->has_state_value_ = true;
    this->publish_state(value);
    this->pref_.save(&value);
  }

  void control(float value) override {
    if (this->has_state_value_ && value == this->state) {
      return;
    }

    esp_err_t err = ESP_OK;
    uint32_t as_u32 = static_cast<uint32_t>(value);
    switch (this->type_) {
      case NumberType::OPEN_DURATION:
        err = gdo_set_open_duration(static_cast<uint16_t>(as_u32));
        break;
      case NumberType::CLOSE_DURATION:
        err = gdo_set_close_duration(static_cast<uint16_t>(as_u32));
        break;
      case NumberType::CLIENT_ID:
        err = gdo_set_client_id(as_u32);
        break;
      case NumberType::ROLLING_CODE:
        err = gdo_set_rolling_code(as_u32);
        break;
      default:
        err = ESP_ERR_INVALID_ARG;
        break;
    }

    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
      this->update_state(value);
    } else {
      ESP_LOGW("gdo_number", "Failed to set number value: %s", esp_err_to_name(err));
    }
  }

  void set_type(NumberType type) { this->type_ = type; }
  bool has_state_value() const { return this->has_state_value_; }
  float get_value() const { return this->state; }

 protected:
  NumberType type_{NumberType::OPEN_DURATION};
  ESPPreferenceObject pref_{};
  bool has_state_value_{false};
};

}  // namespace secplus_gdo
}  // namespace esphome

/************************************
 *
 * Copyright (C) 2024 Konnected.io
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once

#include <functional>

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "gdo.h"

namespace esphome {
namespace secplus_gdo {

enum class SwitchType {
  LEARN,
  TOGGLE_ONLY,
};

class GDOSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override {}

  void setup() override {
    bool value = false;
    this->pref_ = this->make_entity_preference<bool>();
    if (!this->pref_.load(&value)) {
      value = false;
      this->has_state_value_ = false;
    } else {
      this->has_state_value_ = true;
    }

    if (this->type_ == SwitchType::TOGGLE_ONLY && this->f_control_ != nullptr) {
      this->f_control_(value);
    }

    this->publish_state(value);
  }

  void write_state(bool state) override {
    if (state == this->state) {
      return;
    }

    if (this->type_ == SwitchType::TOGGLE_ONLY) {
      if (this->f_control_ != nullptr) {
        this->f_control_(state);
      }
      this->pref_.save(&state);
      this->has_state_value_ = true;
      this->publish_state(state);
      return;
    }

    if (this->type_ == SwitchType::LEARN) {
      esp_err_t err = state ? gdo_activate_learn() : gdo_deactivate_learn();
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW("gdo_switch", "Failed to set learn state: %s", esp_err_to_name(err));
        return;
      }

      this->publish_state(state);
    }
  }

  void set_type(SwitchType type) { this->type_ = type; }
  void set_control_function(std::function<void(bool)> f) { this->f_control_ = std::move(f); }
  bool has_state_value() const { return this->has_state_value_; }

 protected:
  SwitchType type_{SwitchType::LEARN};
  std::function<void(bool)> f_control_{nullptr};
  ESPPreferenceObject pref_{};
  bool has_state_value_{false};
};

}  // namespace secplus_gdo
}  // namespace esphome

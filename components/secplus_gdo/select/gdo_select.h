/************************************
 *
 * Copyright (C) 2024 Konnected.io
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "../gdolib/gdo.h"

namespace esphome {
namespace secplus_gdo {

class GDOSelect : public select::Select, public Component {
 public:
  void setup() override {
    size_t index = 0;
    this->pref_ = this->make_entity_preference<size_t>();
    if (!this->pref_.load(&index) || !this->has_index(index)) {
      auto initial_index = this->index_of(this->initial_option_);
      index = initial_index.has_value() ? initial_index.value() : 0;
    }

    this->selected_index_ = index;
    this->has_selection_ = true;
    this->publish_state(index);
  }

  void set_initial_option(const std::string &initial_option) { this->initial_option_ = initial_option; }

  void update_state(gdo_protocol_type_t protocol) {
    size_t protocol_idx = static_cast<size_t>(protocol);
    if (!this->has_index(protocol_idx)) {
      return;
    }

    if (this->has_selection_ && this->selected_index_ == protocol_idx) {
      return;
    }

    this->selected_index_ = protocol_idx;
    this->has_selection_ = true;
    this->pref_.save(&protocol_idx);
    this->publish_state(protocol_idx);
  }

  bool has_selected_protocol() const { return this->has_selection_; }

  gdo_protocol_type_t get_selected_protocol() const {
    if (!this->has_selection_ || !this->has_index(this->selected_index_)) {
      return (gdo_protocol_type_t) 0;
    }
    return static_cast<gdo_protocol_type_t>(this->selected_index_);
  }

 protected:
  void control(size_t index) override {
    if (!this->has_index(index)) {
      ESP_LOGW("gdo_select", "Invalid protocol index: %u", static_cast<unsigned int>(index));
      return;
    }

    auto protocol = static_cast<gdo_protocol_type_t>(index);
    esp_err_t err = gdo_set_protocol(protocol);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW("gdo_select", "Failed to set protocol: %s", esp_err_to_name(err));
      return;
    }

    this->selected_index_ = index;
    this->has_selection_ = true;
    this->pref_.save(&index);
    this->publish_state(index);
  }

  std::string initial_option_{"auto"};
  ESPPreferenceObject pref_{};
  size_t selected_index_{0};
  bool has_selection_{false};
};

}  // namespace secplus_gdo
}  // namespace esphome

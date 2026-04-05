/************************************
 *
 * Copyright (C) 2024 Konnected.io
 * Copyright (C) 2026  CircuitSetup
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "gdo.h"

namespace esphome {
namespace secplus_gdo {

    class GDOSelect : public select::Select, public Component {
    public:
        void dump_config() override {
            ESP_LOGCONFIG(TAG, "GDO select initial option: %s", this->initial_option_.c_str());
        }

        void setup() override {
            std::string value;
            size_t index;
            this->pref_ = this->make_entity_preference<size_t>();
            if (!this->pref_.load(&index)) {
                value = this->initial_option_;
            } else if (!this->has_index(index)) {
                value = this->initial_option_;
            } else {
                value = this->at(index).value();
            }

            this->apply_option_(value, false);
        }

        void set_initial_option(const std::string &initial_option) { this->initial_option_ = initial_option; }

        void update_state(gdo_protocol_type_t protocol) {
            if (this->has_index(protocol)) {
                std::string value = this->at(protocol).value();
                if (this->has_state() && value != this->current_option()) {
                    const auto index = static_cast<size_t>(protocol);
                    this->pref_.save(&index);
                }

                this->publish_state(value);
            }
        }

    protected:
        void control(const std::string &value) override {
            this->apply_option_(value, true);
        }

        void apply_option_(const std::string &value, bool save_preference) {
            auto idx = this->index_of(value);
            if (!idx.has_value()) {
                ESP_LOGE(TAG, "Unsupported protocol option: %s", value.c_str());
                return;
            }

            gdo_protocol_type_t protocol = static_cast<gdo_protocol_type_t>(idx.value());
            const auto err = gdo_set_protocol(protocol);
            const bool unchanged = this->has_state() && value == this->current_option();
            if (err != ESP_OK && !(unchanged && err == ESP_ERR_INVALID_STATE)) {
                ESP_LOGE(TAG, "Failed to set protocol to %s: %s", value.c_str(), esp_err_to_name(err));
                return;
            }

            if (save_preference) {
                const auto index = static_cast<size_t>(idx.value());
                this->pref_.save(&index);
            }

            this->publish_state(value);
        }

        std::string initial_option_;
        ESPPreferenceObject pref_;
        static constexpr const char *TAG = "gdo.select";
    };

} // namespace secplus_gdo
} // namespace esphome

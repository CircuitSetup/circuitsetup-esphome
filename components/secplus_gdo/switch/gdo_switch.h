/************************************
 *
 * Copyright (C) 2024 Konnected.io
 * Copyright (C) 2026  CircuitSetup
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once

#include <cstdint>
#include <utility>

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "gdo.h"

namespace esphome {
namespace secplus_gdo {

    enum class SwitchType : uint8_t {
        LEARN = 0,
        TOGGLE_ONLY = 1,
    };

    class GDOSwitch : public switch_::Switch, public Component {
    public:
        void dump_config() override { ESP_LOGCONFIG(TAG, "GDO switch type: %s", this->type_to_string_()); }

        void setup() override {
            bool value = false;
            this->pref_ = this->make_entity_preference<bool>();
            if (!this->pref_.load(&value)) {
                value = false;
            }

            if (this->type_ == SwitchType::TOGGLE_ONLY) {
                if (this->f_control) {
                    this->f_control(value);
                }
                this->publish_state(value);
            } else {
                this->publish_state(false);
            }
        }

        void write_state(bool state) override {
            if (state == this->state) {
                return;
            }

            if (this->type_ == SwitchType::TOGGLE_ONLY) {
                if (this->f_control) {
                    this->f_control(state);
                }
                this->pref_.save(&state);
                this->publish_state(state);
                return;
            }

            const auto err = state ? gdo_activate_learn() : gdo_deactivate_learn();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set %s: %s", this->type_to_string_(), esp_err_to_name(err));
                return;
            }

            this->publish_state(state);
        }

        void publish_state_from_device(bool state) {
            if (state != this->state) {
                this->publish_state(state);
            }
        }

        void set_type(uint8_t type) { this->type_ = static_cast<SwitchType>(type); }
        SwitchType get_type() const { return this->type_; }
        void set_control_function(std::function<void(bool)> f) { this->f_control = std::move(f); }

    protected:
        const char *type_to_string_() const {
            switch (this->type_) {
            case SwitchType::LEARN:
                return "learn";
            case SwitchType::TOGGLE_ONLY:
                return "toggle_only";
            default:
                return "unknown";
            }
        }

        SwitchType               type_{SwitchType::LEARN};
        std::function<void(bool)> f_control{nullptr};
        ESPPreferenceObject       pref_;
        static constexpr const char *TAG = "gdo.switch";
    };

} // namespace secplus_gdo
} // namespace esphome

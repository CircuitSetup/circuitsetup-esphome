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

#include <functional>

#include "automation.h"
#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "gdo.h"
#include "inttypes.h"

namespace esphome {
namespace secplus_gdo {

class GDOComponent;

using namespace esphome::cover;
    class GDODoor : public cover::Cover, public Component {
    public:
        void dump_config() override {
            ESP_LOGCONFIG(TAG, "GDO cover configured");
            ESP_LOGCONFIG(TAG, "  Pre-close warning duration: %" PRIu32 " ms", this->pre_close_duration_);
            ESP_LOGCONFIG(TAG, "  Toggle-only mode: %s", this->toggle_only_ ? "YES" : "NO");
        }

        [[nodiscard]] cover::CoverTraits get_traits() override {
            CoverTraits traits;
            traits.set_supports_stop(true);
            traits.set_supports_toggle(true);
            traits.set_supports_position(true);
            return traits;
        }

        void register_door_closing_warn_start_trigger(CoverClosingStartTrigger *trigger) {
            this->pre_close_start_trigger = trigger;
        }

        void register_door_closing_warn_end_trigger(CoverClosingEndTrigger *trigger) {
            this->pre_close_end_trigger = trigger;
        }

        void set_sync_state(bool synced) { this->synced_ = synced; }

        bool do_action(const cover::CoverCall &call);
        bool do_action_after_warning(cover::CoverCall call);
        void set_pre_close_warning_duration(uint32_t ms) { this->pre_close_duration_ = ms; }
        void set_toggle_only(bool val) { this->toggle_only_ = val; }
        void set_state(gdo_door_state_t state, float position);
        void cancel_pre_close_warning();
        void set_parent(GDOComponent *parent) { this->parent_ = parent; }

    protected:
        void control(const cover::CoverCall &call) override;
        bool send_command_(const char *action, std::function<esp_err_t()> &&command);
        void remember_pre_close_state_();
        void restore_pre_close_state_();
        void clear_pre_close_state_();

        CoverClosingStartTrigger *pre_close_start_trigger{nullptr};
        CoverClosingEndTrigger   *pre_close_end_trigger{nullptr};
        uint32_t                  pre_close_duration_{0};
        bool                      pre_close_active_{false};
        bool                      toggle_only_{false};
        optional<float>           target_position_{};
        CoverOperation            prev_operation{COVER_OPERATION_IDLE};
        gdo_door_state_t          state_{GDO_DOOR_STATE_UNKNOWN};
        bool                      synced_{false};
        GDOComponent             *parent_{nullptr};
        gdo_door_state_t          pre_close_restore_state_{GDO_DOOR_STATE_UNKNOWN};
        float                     pre_close_restore_position_{COVER_OPEN};
        CoverOperation            pre_close_restore_operation_{COVER_OPERATION_IDLE};
        bool                      has_pre_close_restore_{false};
        static constexpr const char *TAG = "gdo_cover";
    };

} // namespace secplus_gdo
} // namespace esphome

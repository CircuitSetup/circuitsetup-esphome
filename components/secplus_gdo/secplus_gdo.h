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

#include "binary_sensor/gdo_binary_sensor.h"
#include "cover/gdo_door.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "gdo.h"
#include "light/gdo_light.h"
#include "lock/gdo_lock.h"
#include "number/gdo_number.h"
#include "select/gdo_select.h"
#include "sensor/gdo_sensor.h"
#include "switch/gdo_switch.h"
#include "text_sensor/gdo_text_sensor.h"

namespace esphome {
namespace secplus_gdo {

    class GDOComponent : public Component {
    public:
        void setup() override;
        void loop() override {}
        void dump_config() override;
        void on_shutdown() override;
        void start_gdo();

        // Initialize the driver early, then defer gdo_start() until child entities have restored preferences.
        [[nodiscard]] float get_setup_priority() const override { return setup_priority::HARDWARE; }

        void register_protocol_select(GDOSelect *select) { this->protocol_select_ = select; }
        void set_protocol_state(gdo_protocol_type_t protocol) {
            if (this->protocol_select_ != nullptr) {
                this->protocol_select_->update_state(protocol);
            }
        }

        void register_binary_sensor(GDOBinarySensor *sensor);
        void register_sensor(GDOStat *sensor);
        void register_text_sensor(GDOTextSensor *sensor);
        void register_number(GDONumber *num);
        void register_switch(GDOSwitch *sw);

        void notify_cover_command() { this->cover_triggered_ = true; }

        void set_motion_state(gdo_motion_state_t state);
        void set_obstruction(gdo_obstruction_state_t state);
        void set_button_state(gdo_button_state_t state);
        void set_motor_state(gdo_motor_state_t state);
        void set_battery_state(gdo_battery_state_t state);
        void set_openings(uint16_t openings);
        void set_paired_devices(const gdo_paired_device_t &paired_devices);

        void register_door(GDODoor *door) {
            this->door_ = door;
            if (door != nullptr) {
                door->set_parent(this);
            }
        }
        void set_door_state(gdo_door_state_t state, float position) {
            if (this->door_ != nullptr) {
                this->door_->set_state(state, position);
            }
        }

        void register_light(GDOLight *light) { this->light_ = light; }
        void set_light_state(gdo_light_state_t state) {
            if (this->light_ != nullptr) {
                this->light_->set_state(state);
            }
        }

        void register_lock(GDOLock *lock) { this->lock_ = lock; }
        void set_lock_state(gdo_lock_state_t state) {
            if (this->lock_ != nullptr) {
                this->lock_->set_state(state);
            }
        }

        void set_learn_state(gdo_learn_state_t state) {
            if (this->learn_switch_ != nullptr) {
                this->learn_switch_->publish_state_from_device(state == GDO_LEARN_STATE_ACTIVE);
            }
        }

        void set_open_duration(uint16_t ms) {
            if (this->open_duration_ != nullptr) {
                this->open_duration_->update_state(ms);
            }
        }
        void set_close_duration(uint16_t ms) {
            if (this->close_duration_ != nullptr) {
                this->close_duration_->update_state(ms);
            }
        }
        void set_client_id(uint32_t num) {
            if (this->client_id_ != nullptr) {
                this->client_id_->update_state(num);
            }
        }
        void set_rolling_code(uint32_t num) {
            if (this->rolling_code_ != nullptr) {
                this->rolling_code_->update_state(num);
            }
        }

        void set_sync_state(bool synced);

    protected:
        void sync_toggle_only_();
        void start_if_ready_();

        gdo_status_t      status_{};
        GDOBinarySensor  *motion_sensor_{nullptr};
        GDOBinarySensor  *obstruction_sensor_{nullptr};
        GDOBinarySensor  *motor_sensor_{nullptr};
        GDOBinarySensor  *button_sensor_{nullptr};
        GDOBinarySensor  *sync_sensor_{nullptr};
        GDOBinarySensor  *wireless_remote_sensor_{nullptr};
        GDODoor          *door_{nullptr};
        GDOLight         *light_{nullptr};
        GDOLock          *lock_{nullptr};
        GDOTextSensor    *battery_sensor_{nullptr};
        GDOStat          *openings_sensor_{nullptr};
        GDOStat          *paired_total_sensor_{nullptr};
        GDOStat          *paired_remotes_sensor_{nullptr};
        GDOStat          *paired_keypads_sensor_{nullptr};
        GDOStat          *paired_wall_controls_sensor_{nullptr};
        GDOStat          *paired_accessories_sensor_{nullptr};
        GDONumber        *open_duration_{nullptr};
        GDONumber        *close_duration_{nullptr};
        GDONumber        *client_id_{nullptr};
        GDONumber        *rolling_code_{nullptr};
        GDOSelect        *protocol_select_{nullptr};
        GDOSwitch        *learn_switch_{nullptr};
        GDOSwitch        *toggle_only_switch_{nullptr};
        bool              initialized_{false};
        bool              start_requested_{true};
        bool              started_{false};
        bool              cover_triggered_{false};
        bool              button_triggered_{false};

    }; // GDOComponent

} // namespace secplus_gdo
} // namespace esphome

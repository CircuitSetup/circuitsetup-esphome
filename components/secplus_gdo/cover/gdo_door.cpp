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

#include "gdo_door.h"

#include <functional>
#include <utility>

#include "../secplus_gdo.h"
#include "esphome/core/log.h"
#include "inttypes.h"

namespace esphome {
namespace secplus_gdo {

// How long after sending a close to wait for the opener to report motion before
// assuming it refused the command (e.g. safety beam blocked).
static const uint32_t CLOSE_WATCHDOG_MS = 10000;

void GDODoor::set_state(gdo_door_state_t state, float position) {
    if (this->pre_close_active_) {
        // External motion or a completed close supersedes the delayed command.
        if (state == GDO_DOOR_STATE_CLOSING || state == GDO_DOOR_STATE_OPENING ||
            state == GDO_DOOR_STATE_CLOSED) {
            this->cancel_timeout("pre_close");
            this->pre_close_active_ = false;
            if (this->pre_close_end_trigger) {
                this->pre_close_end_trigger->trigger();
            }
            this->clear_pre_close_state_();
        } else {
            // Keep the warning visible, but cancellation must restore the latest real state.
            if (state == GDO_DOOR_STATE_OPEN || state == GDO_DOOR_STATE_STOPPED) {
                this->state_ = this->pre_close_restore_state_ = state;
                this->pre_close_restore_position_ = state == GDO_DOOR_STATE_OPEN ? COVER_OPEN : position;
                this->pre_close_restore_operation_ = COVER_OPERATION_IDLE;
            }
            return;
        }
    }

    ESP_LOGI(TAG, "Door state: %s, position: %.0f%%", gdo_door_state_to_string(state), position * 100.0f);
#ifdef USE_MQTT
    const float previous_position = this->position;
    const auto previous_operation = this->current_operation;
#endif

    switch (state) {
    case GDO_DOOR_STATE_OPEN:
        this->position = COVER_OPEN;
        this->current_operation = COVER_OPERATION_IDLE;
        break;
    case GDO_DOOR_STATE_CLOSED:
        this->position = COVER_CLOSED;
        this->current_operation = COVER_OPERATION_IDLE;
        break;
    case GDO_DOOR_STATE_OPENING:
        this->current_operation = COVER_OPERATION_OPENING;
        this->last_motion_ = COVER_OPERATION_OPENING;
        this->position = position;
        break;
    case GDO_DOOR_STATE_CLOSING:
        this->current_operation = COVER_OPERATION_CLOSING;
        this->last_motion_ = COVER_OPERATION_CLOSING;
        this->position = position;
        break;
    case GDO_DOOR_STATE_STOPPED: // falls through
    case GDO_DOOR_STATE_MAX: // falls through
    default:
        this->current_operation = COVER_OPERATION_IDLE;
        this->position = position;
        break;
    }

#ifdef USE_MQTT // if MQTT component is enabled, do not publish the same state more than once
    if (this->state_ == state && this->current_operation == previous_operation &&
        this->position == previous_position) {
        return;
    }
#endif

    this->publish_state(false);
    this->state_ = state;
}

bool GDODoor::send_command_(const char *action, std::function<esp_err_t()> &&command) {
    if (!this->synced_) {
        return false;
    }
    const auto err = command();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", action, esp_err_to_name(err));
        return false;
    }
    return true;
}

void GDODoor::remember_pre_close_state_() {
    this->pre_close_restore_state_ = this->state_;
    this->pre_close_restore_position_ = this->position;
    this->pre_close_restore_operation_ = this->current_operation;
    this->has_pre_close_restore_ = true;
}

void GDODoor::restore_pre_close_state_() {
    if (!this->has_pre_close_restore_) {
        return;
    }

    this->state_ = this->pre_close_restore_state_;
    this->position = this->pre_close_restore_position_;
    this->current_operation = this->pre_close_restore_operation_;
    this->publish_state(false);
    this->clear_pre_close_state_();
}

void GDODoor::clear_pre_close_state_() {
    this->has_pre_close_restore_ = false;
}

bool GDODoor::do_action_after_warning(cover::CoverCall call) {
    if (this->pre_close_active_) {
        return false;
    }

    this->remember_pre_close_state_();
    this->cancel_timeout("close_watchdog");
    // The UI can show Closing without replacing the opener's last state/direction.
    this->current_operation = COVER_OPERATION_CLOSING;
    this->publish_state(false);

    ESP_LOGD(TAG, "WARNING for %" PRIu32 "ms", this->pre_close_duration_);
    if (this->pre_close_start_trigger) {
        this->pre_close_start_trigger->trigger();
    }

    this->set_timeout("pre_close", this->pre_close_duration_, [this, call = std::move(call)]() mutable {
        this->pre_close_active_ = false;
        if (this->pre_close_end_trigger) {
            this->pre_close_end_trigger->trigger();
        }
        if (!this->do_action(call)) {
            this->restore_pre_close_state_();
        } else {
            this->clear_pre_close_state_();
            this->set_timeout("close_watchdog", CLOSE_WATCHDOG_MS, [this]() { this->check_close_started(); });
        }
    });

    this->pre_close_active_ = true;
    return true;
}

// The CLOSING state set before the pre-close warning is optimistic. If the opener refuses
// the close, its door state never changes, so gdolib sends no door event and nothing would
// ever revert it -- the cover sits at "Closing" and control() then ignores both Open
// ("already open") and Close ("already moving"). Re-sync to gdolib's actual state instead.
void GDODoor::check_close_started() {
    if (this->current_operation != COVER_OPERATION_CLOSING) {
        return; // a real door event already moved us off the optimistic state
    }

    gdo_status_t status;
    if (gdo_get_status(&status) != ESP_OK || status.door == GDO_DOOR_STATE_CLOSING ||
        status.door == GDO_DOOR_STATE_UNKNOWN || status.door >= GDO_DOOR_STATE_MAX) {
        return;
    }

    float position = status.door_position < 0 ? this->position : (float)(10000 - status.door_position) / 10000.0f;
    ESP_LOGW(TAG, "Opener did not start closing; restoring door state %s", gdo_door_state_to_string(status.door));
    this->set_state(status.door, position);
}

bool GDODoor::do_action(const cover::CoverCall &call) {
    if (this->parent_) {
        this->parent_->notify_cover_command();
    }

    if (call.get_toggle()) {
        ESP_LOGD(TAG, "Sending TOGGLE action");
        return this->send_command_("door toggle", []() { return gdo_door_toggle(); });
    }

    if (!call.get_position().has_value()) {
        return false;
    }

    auto pos = *call.get_position();
    if (pos == COVER_OPEN) {
        if (this->toggle_only_) {
            ESP_LOGD(TAG, "Sending TOGGLE action");
            if (!this->send_command_("door toggle", []() { return gdo_door_toggle(); })) {
                return false;
            }
            if (this->state_ == GDO_DOOR_STATE_STOPPED && this->last_motion_ == COVER_OPERATION_OPENING) {
                // If the door was stopped while opening, then we need to toggle to stop, then toggle again to open,
                this->set_timeout("stop_door", 1000, [this]() {
                    const auto err = gdo_door_stop();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door stop failed: %s", esp_err_to_name(err));
                        this->cancel_timeout("open_door");
                    }
                });
                this->set_timeout("open_door", 2000, []() {
                    const auto err = gdo_door_toggle();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door reopen toggle failed: %s", esp_err_to_name(err));
                    }
                });
            }
            return true;
        }

        ESP_LOGD(TAG, "Sending OPEN action");
        return this->send_command_("door open", []() { return gdo_door_open(); });
    }

    if (pos == COVER_CLOSED) {
        if (this->toggle_only_) {
            ESP_LOGD(TAG, "Sending TOGGLE action");
            if (!this->send_command_("door toggle", []() { return gdo_door_toggle(); })) {
                return false;
            }
            if (this->state_ == GDO_DOOR_STATE_STOPPED && this->last_motion_ == COVER_OPERATION_CLOSING) {
                // If the door was stopped while closing, then we need to toggle to stop, then toggle again to close,
                this->set_timeout("stop_door", 1000, [this]() {
                    const auto err = gdo_door_stop();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door stop failed: %s", esp_err_to_name(err));
                        this->cancel_timeout("close_door");
                    }
                });
                this->set_timeout("close_door", 2000, []() {
                    const auto err = gdo_door_toggle();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door reclose toggle failed: %s", esp_err_to_name(err));
                    }
                });
            }
            return true;
        }

        ESP_LOGD(TAG, "Sending CLOSE action");
        return this->send_command_("door close", []() { return gdo_door_close(); });
    }

    ESP_LOGD(TAG, "Moving garage door to position %f", pos);
    return this->send_command_("door move_to_target", [pos]() {
        return gdo_door_move_to_target(static_cast<uint32_t>(10000 - (pos * 10000)));
    });
}

void GDODoor::control(const cover::CoverCall &call) {
    this->cancel_movement_timeouts_();
    if (!this->synced_) {
        ESP_LOGW(TAG, "Ignoring cover command while opener is not synced");
        this->publish_state(false);
        return;
    }

    if (call.get_stop()) {
        ESP_LOGD(TAG, "Stop command received");
        this->cancel_pre_close_warning();
        if (!this->send_command_("door stop", []() { return gdo_door_stop(); })) {
            this->publish_state(false);
        }
        return;
    }

    if (call.get_toggle()) {
        ESP_LOGD(TAG, "Toggle command received");
        if (this->pre_close_active_) {
            this->cancel_pre_close_warning();
        } else if (this->current_operation == COVER_OPERATION_OPENING ||
                   this->current_operation == COVER_OPERATION_CLOSING) {
            if (!this->do_action(call)) {
                this->publish_state(false);
            }
        } else if (this->position != COVER_CLOSED) {
            if (!this->do_action_after_warning(call)) {
                this->publish_state(false);
            }
        } else if (!this->do_action(call)) {
            this->publish_state(false);
        }
        return;
    }

    if (!call.get_position().has_value()) {
        return;
    }

    auto pos = *call.get_position();
    if (this->pre_close_active_) {
        if (pos < this->position) {
            ESP_LOGD(TAG, "Door is already closing");
            this->publish_state(false);
            return;
        }
        this->cancel_pre_close_warning();
    }

    if (this->position == pos) {
        if (pos == COVER_OPEN) {
            ESP_LOGD(TAG, "Door is already open");
        } else if (pos == COVER_CLOSED) {
            ESP_LOGD(TAG, "Door is already closed");
        } else {
            ESP_LOGD(TAG, "Door is already at %.0f%%", pos * 100.0f);
        }
        this->publish_state(false);
        return;
    }

    if ((this->current_operation == COVER_OPERATION_OPENING && pos > this->position) ||
        (this->current_operation == COVER_OPERATION_CLOSING && pos < this->position)) {
        ESP_LOGD(TAG, "Door is already moving in target direction; target position: %.0f%%", pos * 100.0f);
        this->publish_state(false);
        return;
    }

    if (this->current_operation == COVER_OPERATION_OPENING ||
        this->current_operation == COVER_OPERATION_CLOSING) {
        ESP_LOGD(TAG, "Door is in motion - Sending STOP action");
        if (!this->send_command_("door stop", []() { return gdo_door_stop(); })) {
            this->publish_state(false);
            return;
        }
    }

    bool action_started = false;
    if (pos == COVER_OPEN) {
        ESP_LOGD(TAG, "Open command received");
        action_started = this->do_action(call);
    } else if (pos == COVER_CLOSED) {
        ESP_LOGD(TAG, "Close command received");
        action_started = this->do_action_after_warning(call);
    } else {
        ESP_LOGD(TAG, "Move to position %f command received", pos);
        if (pos < this->position) {
            ESP_LOGV(TAG, "Current position: %f; Intended position: %f; Door will move down after warning", this->position, pos);
            action_started = this->do_action_after_warning(call);
        } else {
            ESP_LOGV(TAG, "Current position: %f; Intended position: %f; Door will move up immediately", this->position, pos);
            action_started = this->do_action(call);
        }
    }

    if (!action_started) {
        this->publish_state(false);
    }
}

void GDODoor::cancel_movement_timeouts_() {
    this->cancel_timeout("stop_door");
    this->cancel_timeout("open_door");
    this->cancel_timeout("close_door");
}

void GDODoor::cancel_pre_close_warning() {
    this->cancel_movement_timeouts_();
    if (!this->pre_close_active_) {
        return;
    }

    ESP_LOGD(TAG, "Canceling pending pre-close warning");
    this->cancel_timeout("pre_close");
    this->pre_close_active_ = false;
    if (this->pre_close_end_trigger) {
        this->pre_close_end_trigger->trigger();
    }
    this->restore_pre_close_state_();
}

} // namespace secplus_gdo
} // namespace esphome

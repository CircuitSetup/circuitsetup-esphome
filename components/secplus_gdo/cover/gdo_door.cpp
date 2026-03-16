#include "gdo_door.h"

#include <functional>
#include <utility>

#include "../secplus_gdo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

void GDODoor::set_state(gdo_door_state_t state, float position) {
    if (this->pre_close_active_) {
        // If we are in the pre-close state and the door is closing,
        // then it was triggered by something else and we need to cancel the pre-close
        if (state == GDO_DOOR_STATE_CLOSING) {
            this->cancel_timeout("pre_close");
            this->pre_close_active_ = false;
            if (this->pre_close_end_trigger) {
                this->pre_close_end_trigger->trigger();
            }
            this->clear_pre_close_state_();
        } else {
            // If we are in the pre-close state, and the door is not closing, then do not update
            // the state so that the stop button remains active on the front end to cancel the closing operation
            return;
        }
    }

    ESP_LOGI(TAG, "Door state: %s, position: %.0f%%", gdo_door_state_to_string(state), position * 100.0f);
    this->prev_operation = this->current_operation; // save the previous operation

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
        this->position = position;
        break;
    case GDO_DOOR_STATE_CLOSING:
        this->current_operation = COVER_OPERATION_CLOSING;
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
    if (this->state_ == state && this->current_operation == this->prev_operation) {
        return;
    }
#endif

    this->publish_state(false);
    this->state_ = state;
}

bool GDODoor::send_command_(const char *action, std::function<esp_err_t()> &&command) {
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
    this->set_state(GDO_DOOR_STATE_CLOSING, this->position);

    ESP_LOGD(TAG, "WARNING for %dms", this->pre_close_duration_);
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
        }
    });

    this->pre_close_active_ = true;
    return true;
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
            if (this->state_ == GDO_DOOR_STATE_STOPPED && this->prev_operation == COVER_OPERATION_OPENING) {
                // If the door was stopped while opening, then we need to toggle to stop, then toggle again to open,
                this->set_timeout("stop_door", 1000, []() {
                    const auto err = gdo_door_stop();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door stop failed: %s", esp_err_to_name(err));
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
            if (this->state_ == GDO_DOOR_STATE_STOPPED && this->prev_operation == COVER_OPERATION_CLOSING) {
                // If the door was stopped while closing, then we need to toggle to stop, then toggle again to close,
                this->set_timeout("stop_door", 1000, []() {
                    const auto err = gdo_door_stop();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "door stop failed: %s", esp_err_to_name(err));
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
        if (this->position != COVER_CLOSED) {
            if (this->do_action_after_warning(call)) {
                this->target_position_ = COVER_CLOSED;
            } else {
                this->publish_state(false);
            }
        } else if (this->do_action(call)) {
            this->target_position_ = COVER_OPEN;
        } else {
            this->publish_state(false);
        }
        return;
    }

    if (!call.get_position().has_value()) {
        return;
    }

    auto pos = *call.get_position();
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

    if (this->pre_close_active_) {
        // don't start the pre-close again if the door is already going to close.
        if (pos < this->position) {
            ESP_LOGD(TAG, "Door is already closing");
            this->publish_state(false);
            return;
        }

        ESP_LOGD(TAG, "Canceling pending action");
        this->cancel_timeout("pre_close");
        this->pre_close_active_ = false;
        if (this->pre_close_end_trigger) {
            this->pre_close_end_trigger->trigger();
        }
        this->restore_pre_close_state_();
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

    if (action_started) {
        this->target_position_ = pos;
    } else {
        this->publish_state(false);
    }
}

void GDODoor::cancel_pre_close_warning() {
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

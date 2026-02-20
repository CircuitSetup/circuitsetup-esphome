#include "esphome/core/log.h"
#include "gdo_door.h"
#include "../secplus_gdo.h"
#include <utility>

namespace esphome {
namespace secplus_gdo {

constexpr char TAG[] = "gdo_cover";
using namespace esphome::cover;
static constexpr uint32_t PRE_CLOSE_TIMEOUT_ID = 0x47444F41;
static constexpr uint32_t STOP_DOOR_TIMEOUT_ID = 0x47444F42;
static constexpr uint32_t OPEN_DOOR_TIMEOUT_ID = 0x47444F43;
static constexpr uint32_t CLOSE_DOOR_TIMEOUT_ID = 0x47444F44;

void GDODoor::set_state(gdo_door_state_t state, float position) {
    if (this->pre_close_active_) {
        // If we are in the pre-close state and the door is closing,
        // then it was triggered by something else and we need to cancel the pre-close
        if (state == GDO_DOOR_STATE_CLOSING) {
            this->cancel_timeout(PRE_CLOSE_TIMEOUT_ID);
            this->pre_close_active_ = false;
            if (this->pre_close_end_trigger) {
                this->pre_close_end_trigger->trigger();
            }
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
    if (this->state_ == state && this->current_operation == this->prev_operation) { return; }
    #endif

    this->publish_state(false);
    this->state_ = state;
}

void GDODoor::do_action_after_warning(cover::CoverCall call) {

    if (this->pre_close_active_) {
        return;
    }

    this->set_state(GDO_DOOR_STATE_CLOSING, this->position);

    ESP_LOGD(TAG, "WARNING for %dms", this->pre_close_duration_);
    if (this->pre_close_start_trigger) {
        this->pre_close_start_trigger->trigger();
    }

    this->set_timeout(PRE_CLOSE_TIMEOUT_ID, this->pre_close_duration_, [this, call = std::move(call)]() {
        this->pre_close_active_ = false;
        if (this->pre_close_end_trigger) {
            this->pre_close_end_trigger->trigger();
        }
        this->do_action(call);
    });

    this->pre_close_active_ = true;
}

void GDODoor::do_action(const cover::CoverCall& call) {
    if (this->parent_) {
        this->parent_->notify_cover_command();
    }
    if (call.get_toggle()) {
        ESP_LOGD(TAG, "Sending TOGGLE action");
        if (this->parent_) {
            this->parent_->door_toggle();
        } else {
            gdo_door_toggle();
        }
        return;
    }

    if (call.get_position().has_value()) {
        auto pos = *call.get_position();
        if (pos == COVER_OPEN) {
            if (this->toggle_only_) {
                ESP_LOGD(TAG, "Sending TOGGLE action");
                if (this->parent_) {
                    this->parent_->door_toggle();
                } else {
                    gdo_door_toggle();
                }
                if (this->state_ == GDO_DOOR_STATE_STOPPED && this->prev_operation == COVER_OPERATION_OPENING) {
                    // If the door was stopped while opening, then we need to toggle to stop, then toggle again to open,
                    this->set_timeout(STOP_DOOR_TIMEOUT_ID, 1000, [this]() {
                        if (this->parent_) {
                            this->parent_->door_stop();
                        } else {
                            gdo_door_stop();
                        }
                    });
                    this->set_timeout(OPEN_DOOR_TIMEOUT_ID, 2000, [this]() {
                        if (this->parent_) {
                            this->parent_->door_toggle();
                        } else {
                            gdo_door_toggle();
                        }
                    });
                }
            } else {
                ESP_LOGD(TAG, "Sending OPEN action");
                if (this->parent_) {
                    this->parent_->door_open();
                } else {
                    gdo_door_open();
                }
            }
        } else if (pos == COVER_CLOSED) {
            if (this->toggle_only_) {
                ESP_LOGD(TAG, "Sending TOGGLE action");
                if (this->parent_) {
                    this->parent_->door_toggle();
                } else {
                    gdo_door_toggle();
                }
                if (this->state_ == GDO_DOOR_STATE_STOPPED && this->prev_operation == COVER_OPERATION_CLOSING) {
                    // If the door was stopped while closing, then we need to toggle to stop, then toggle again to close,
                    this->set_timeout(STOP_DOOR_TIMEOUT_ID, 1000, [this]() {
                        if (this->parent_) {
                            this->parent_->door_stop();
                        } else {
                            gdo_door_stop();
                        }
                    });
                    this->set_timeout(CLOSE_DOOR_TIMEOUT_ID, 2000, [this]() {
                        if (this->parent_) {
                            this->parent_->door_toggle();
                        } else {
                            gdo_door_toggle();
                        }
                    });
                }
            } else {
                ESP_LOGD(TAG, "Sending CLOSE action");
                if (this->parent_) {
                    this->parent_->door_close();
                } else {
                    gdo_door_close();
                }
            }
        } else {
            ESP_LOGD(TAG, "Moving garage door to position %f", pos);
            if (this->parent_) {
                this->parent_->door_move_to(pos);
            } else {
                gdo_door_move_to_target(10000 - (pos * 10000));
            }
        }
    }
}

void GDODoor::control(const cover::CoverCall& call) {
    if (!this->synced_) {
        this->publish_state(false);
        return;
    }

    if (call.get_stop()) {
        ESP_LOGD(TAG, "Stop command received");
        this->cancel_pre_close_warning();
        if (this->parent_) {
            this->parent_->door_stop();
        } else {
            gdo_door_stop();
        }
        return;
    }

    if (call.get_toggle()) {
        ESP_LOGD(TAG, "Toggle command received");
        if (this->position != COVER_CLOSED) {
            this->target_position_ = COVER_CLOSED;
            this->do_action_after_warning(call);
        } else {
            this->target_position_ = COVER_OPEN;
            this->do_action(call);
        }

        return;
    }

    if (call.get_position().has_value()) {
        auto pos = *call.get_position();
        if (this->position == pos) {
            ESP_LOGD(TAG, "Door is already %s", pos == COVER_OPEN ? "open" : "closed");
            this->publish_state(false);
            return;
        }

        if ((this->current_operation == COVER_OPERATION_OPENING && pos > this->position) ||
            (this->current_operation == COVER_OPERATION_CLOSING && pos < this->position)) {
            float target = this->target_position_.has_value() ? *this->target_position_ : this->position;
            ESP_LOGD(TAG, "Door is already moving in target direction; target position: %.0f%%", target * 100.0f);
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
            this->cancel_timeout(PRE_CLOSE_TIMEOUT_ID);
            this->pre_close_active_ = false;
            if (this->pre_close_end_trigger) {
                this->pre_close_end_trigger->trigger();
            }
        }

        if (this->current_operation == COVER_OPERATION_OPENING ||
            this->current_operation == COVER_OPERATION_CLOSING) {
            ESP_LOGD(TAG, "Door is in motion - Sending STOP action");
            if (this->parent_) {
                this->parent_->door_stop();
            } else {
                gdo_door_stop();
            }
        }

        if (pos == COVER_OPEN) {
            ESP_LOGD(TAG, "Open command received");
            this->do_action(call);
        } else if (pos == COVER_CLOSED) {
            ESP_LOGD(TAG, "Close command received");
            this->do_action_after_warning(call);
        } else {
            ESP_LOGD(TAG, "Move to position %f command received", pos);
            if (pos < this->position) {
                ESP_LOGV(TAG, "Current position: %f; Intended position: %f; Door will move down after warning", this->position, pos);
                this->do_action_after_warning(call);
            } else {
                ESP_LOGV(TAG, "Current position: %f; Intended position: %f; Door will move up immediately", this->position, pos);
                this->do_action(call);
            }
        }

        this->target_position_ = pos;
    }
}


void GDODoor::cancel_pre_close_warning() {
    if (this->pre_close_active_) {
        ESP_LOGD(TAG, "Canceling pending pre-close warning");
        this->cancel_timeout(PRE_CLOSE_TIMEOUT_ID);
        this->pre_close_active_ = false;
        if (this->pre_close_end_trigger) {
            this->pre_close_end_trigger->trigger();
        }
        this->set_state(GDO_DOOR_STATE_OPEN, this->position);
        this->publish_state(false);
    }
}

} // namespace secplus_gdo
} // namespace esphome

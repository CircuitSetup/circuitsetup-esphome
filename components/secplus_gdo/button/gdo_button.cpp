#include "gdo_button.h"

#include "../secplus_gdo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

static constexpr const char *const TAG = "gdo_cmd_button";

void GDOCommandButton::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Button has no parent component");
    return;
  }

  switch (this->type_) {
    case CommandButtonType::SYNC:
      this->parent_->request_sync();
      break;
    case CommandButtonType::RESYNC:
      this->parent_->resync_opener();
      break;
    case CommandButtonType::RESTART_COMMS:
      this->parent_->restart_comms();
      break;
    case CommandButtonType::RESET_TRAVEL_TIMINGS:
      this->parent_->reset_travel_timings();
      break;
    case CommandButtonType::DOOR_OPEN:
      this->parent_->door_open();
      break;
    case CommandButtonType::DOOR_CLOSE:
      this->parent_->door_close();
      break;
    case CommandButtonType::DOOR_STOP:
      this->parent_->door_stop();
      break;
    case CommandButtonType::DOOR_TOGGLE:
      this->parent_->door_toggle();
      break;
    case CommandButtonType::LIGHT_TOGGLE:
      this->parent_->light_toggle();
      break;
    case CommandButtonType::LOCK_TOGGLE:
      this->parent_->lock_toggle();
      break;
    default:
      ESP_LOGW(TAG, "Unhandled command button type");
      break;
  }
}

}  // namespace secplus_gdo
}  // namespace esphome

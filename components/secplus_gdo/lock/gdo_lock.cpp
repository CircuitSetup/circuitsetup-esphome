#include "gdo_lock.h"

#include "../secplus_gdo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

void GDOLock::set_state(gdo_lock_state_t state) {
  if (state == this->lock_state_) {
    return;
  }

  this->lock_state_ = state;
  ESP_LOGI(TAG, "Lock state: %s", gdo_lock_state_to_string(state));
  this->publish_state(state == GDO_LOCK_STATE_LOCKED ? lock::LockState::LOCK_STATE_LOCKED
                                                      : lock::LockState::LOCK_STATE_UNLOCKED);
}

void GDOLock::control(const lock::LockCall &call) {
  if (!this->synced_) {
    return;
  }

  if (!call.get_state().has_value()) {
    return;
  }

  auto state = *call.get_state();

  if (this->parent_ != nullptr) {
    if (state == lock::LockState::LOCK_STATE_LOCKED) {
      this->parent_->lock();
    } else if (state == lock::LockState::LOCK_STATE_UNLOCKED) {
      this->parent_->unlock();
    }
    return;
  }

  if (state == lock::LockState::LOCK_STATE_LOCKED) {
    gdo_lock();
  } else if (state == lock::LockState::LOCK_STATE_UNLOCKED) {
    gdo_unlock();
  }
}

}  // namespace secplus_gdo
}  // namespace esphome

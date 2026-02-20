#include "gdo_light.h"

#include "../secplus_gdo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace secplus_gdo {

void GDOLight::write_state(light::LightState *state) {
  if (!this->synced_) {
    return;
  }

  bool binary;
  state->current_values_as_binary(&binary);
  if (this->parent_ != nullptr) {
    if (binary) {
      this->parent_->light_on();
    } else {
      this->parent_->light_off();
    }
    return;
  }

  if (binary) {
    gdo_light_on();
  } else {
    gdo_light_off();
  }
}

void GDOLight::set_state(gdo_light_state_t state) {
  if (state == this->light_state_) {
    return;
  }

  this->light_state_ = state;
  ESP_LOGI(TAG, "Light state: %s", gdo_light_state_to_string(state));
  if (this->state_ == nullptr) {
    return;
  }

  bool is_on = state == GDO_LIGHT_STATE_ON;
  this->state_->current_values.set_state(is_on);
  this->state_->remote_values.set_state(is_on);
  this->state_->publish_state();
}

}  // namespace secplus_gdo
}  // namespace esphome

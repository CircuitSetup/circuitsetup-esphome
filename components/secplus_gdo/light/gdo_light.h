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

#include "esphome/core/component.h"
#include "esphome/components/binary/light/binary_light_output.h"
#include "../gdolib/gdo.h"

namespace esphome {
namespace secplus_gdo {

class GDOComponent;

class GDOLight : public binary::BinaryLightOutput, public Component {
 public:
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;
  void set_state(gdo_light_state_t state);
  void set_sync_state(bool synced) { this->synced_ = synced; }
  void set_parent(GDOComponent *parent) { this->parent_ = parent; }

 protected:
  light::LightState *state_{nullptr};
  gdo_light_state_t light_state_{GDO_LIGHT_STATE_MAX};
  GDOComponent *parent_{nullptr};
  static constexpr auto TAG{"GDOLight"};
  bool synced_{false};
};

}  // namespace secplus_gdo
}  // namespace esphome

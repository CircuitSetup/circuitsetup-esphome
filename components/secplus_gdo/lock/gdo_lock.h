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

#include "esphome/components/lock/lock.h"
#include "esphome/core/component.h"
#include "gdo.h"

namespace esphome {
namespace secplus_gdo {

class GDOComponent;

class GDOLock : public lock::Lock, public Component {
 public:
  void set_state(gdo_lock_state_t state);
  void control(const lock::LockCall &call) override;
  void set_sync_state(bool synced) { this->synced_ = synced; }
  void set_parent(GDOComponent *parent) { this->parent_ = parent; }

 protected:
  gdo_lock_state_t lock_state_{GDO_LOCK_STATE_MAX};
  bool synced_{false};
  GDOComponent *parent_{nullptr};
  static constexpr const char *TAG = "GDOLock";
};

}  // namespace secplus_gdo
}  // namespace esphome

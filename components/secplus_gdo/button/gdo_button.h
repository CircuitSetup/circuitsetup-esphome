#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome {
namespace secplus_gdo {

class GDOComponent;

enum class CommandButtonType {
  SYNC,
  RESYNC,
  RESTART_COMMS,
  RESET_TRAVEL_TIMINGS,
  DOOR_OPEN,
  DOOR_CLOSE,
  DOOR_STOP,
  DOOR_TOGGLE,
  LIGHT_TOGGLE,
  LOCK_TOGGLE,
};

class GDOCommandButton : public button::Button, public Component {
 public:
  void set_parent(GDOComponent *parent) { this->parent_ = parent; }
  void set_type(CommandButtonType type) { this->type_ = type; }

 protected:
  void press_action() override;

  GDOComponent *parent_{nullptr};
  CommandButtonType type_{CommandButtonType::SYNC};
};

}  // namespace secplus_gdo
}  // namespace esphome

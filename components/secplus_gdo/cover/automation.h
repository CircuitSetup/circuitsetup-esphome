#pragma once

#include "esphome/core/automation.h"

namespace esphome {
namespace secplus_gdo {
    class GDODoor;

    class CoverClosingStartTrigger : public Trigger<> {
    public:
        explicit CoverClosingStartTrigger([[maybe_unused]] GDODoor* gdo_door) {}
    };

    class CoverClosingEndTrigger : public Trigger<> {
    public:
        explicit CoverClosingEndTrigger([[maybe_unused]] GDODoor* gdo_door) {}
    };
} // namespace secplus_gdo
} // namespace esphome


using namespace esphome::secplus_gdo;
using namespace esphome::cover;
struct Door : GDODoor {
    using GDODoor::control;
    Door(gdo_door_state_t state = GDO_DOOR_STATE_OPEN, float pos = 1.0f) {
        opens = stops = toggles = closes = command_error = status_error = status_reads = 0;
        status = {GDO_DOOR_STATE_OPEN, 0};
        set_sync_state(true);
        set_pre_close_warning_duration(5000);
        set_state(state, pos);
    }
};
int failures = 0;
void check(bool ok, const char *label) {
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
int main() {
    CoverCall toggle; toggle.toggle = true;
    CoverCall close; close.target = 0.0f;
    for (auto motion : {GDO_DOOR_STATE_OPENING, GDO_DOOR_STATE_CLOSING}) {
        Door door(motion, 0.5f);
        door.control(toggle);
        check(toggles == 1 && door.timers.empty(), "toggle stops a moving door immediately");
    }
    {
        Door door;
        door.control(close);
        check(closes == 0 && door.timers.at("pre_close").first == 5000,
              "close waits for the configured warning");
        door.control(toggle);
        check(door.timers.empty() && closes == 0 && toggles == 0 &&
              door.current_operation == COVER_OPERATION_IDLE && door.position == 1.0f,
              "toggle cancels the warning and restores Open without sending a command");
    }
    for (bool use_toggle : {false, true}) {
        Door door;
        door.control(use_toggle ? toggle : close);
        door.fire("pre_close");
        check((use_toggle ? toggles : closes) == 1, "close command sent when warning ends");
        check(door.timers.count("close_watchdog") &&
              door.timers.at("close_watchdog").first == 10000,
              "watchdog starts ten seconds after sending close or toggle");
        door.fire("close_watchdog");
        check(door.current_operation == COVER_OPERATION_IDLE && door.position == 1.0f,
              "refused close restores the actual Open state");
    }
    for (auto state : {GDO_DOOR_STATE_CLOSING, GDO_DOOR_STATE_UNKNOWN, GDO_DOOR_STATE_MAX}) {
        Door door;
        door.control(close); door.fire("pre_close");
        status = {state, 5000};
        door.fire("close_watchdog");
        check(door.current_operation == COVER_OPERATION_CLOSING,
              "watchdog ignores confirmed closing and unknown or invalid status");
    }
    {
        Door door;
        door.control(close); door.fire("pre_close");
        status_error = 1;
        door.fire("close_watchdog");
        check(door.current_operation == COVER_OPERATION_CLOSING, "status error preserves cover state");
    }
    for (int position : {2500, -1}) {
        Door door;
        door.control(close); door.fire("pre_close");
        status = {GDO_DOOR_STATE_STOPPED, position};
        door.fire("close_watchdog");
        check(door.current_operation == COVER_OPERATION_IDLE &&
              door.position == (position == -1 ? 1.0f : 0.75f),
              "stopped status converts gdolib position and preserves unknown position");
    }
    {
        Door door;
        door.control(close); door.fire("pre_close");
        door.set_state(GDO_DOOR_STATE_CLOSED, 0.0f);
        door.fire("close_watchdog");
        check(door.position == 0.0f && status_reads == 0, "watchdog leaves a completed close alone");
    }
    {
        Door door;
        door.control(close); door.fire("pre_close");
        door.set_state(GDO_DOOR_STATE_OPEN, 1.0f);
        door.control(close);
        check(!door.timers.count("close_watchdog"), "new close cancels the old watchdog");
    }
    {
        Door door;
        door.control(close); command_error = 1; door.fire("pre_close");
        check(door.current_operation == COVER_OPERATION_IDLE && door.position == 1.0f &&
              !door.timers.count("close_watchdog"), "failed send restores state without a watchdog");
    }

    CoverCall open; open.target = 1.0f;
    CoverCall stop; stop.stop = true;
    {
        Door door;
        door.control(close);
        door.control(open);
        door.fire("pre_close");
        check(closes == 0 && door.current_operation == COVER_OPERATION_IDLE,
              "Open cancels a close warning even while the door is still fully open");
    }
    {
        Door door(GDO_DOOR_STATE_OPENING, 0.5f);
        door.control(close);
        door.set_state(GDO_DOOR_STATE_STOPPED, 0.6f);
        door.control(open);
        door.fire("pre_close");
        check(opens == 1 && closes == 0 && door.position == 0.6f,
              "Open after warning cancellation uses the latest stopped feedback");
    }
    {
        Door door;
        door.control(close);
        door.set_sync_state(false);
        door.set_sync_state(true);
        door.fire("pre_close");
        check(closes == 0 && door.current_operation == COVER_OPERATION_IDLE,
              "sync loss cancels the queued close even if sync recovers before expiry");
    }
    for (auto state : {GDO_DOOR_STATE_OPENING, GDO_DOOR_STATE_CLOSED}) {
        Door door;
        door.control(toggle);
        door.set_state(state, state == GDO_DOOR_STATE_CLOSED ? 0.0f : 0.5f);
        door.fire("pre_close");
        check(toggles == 0 && (state != GDO_DOOR_STATE_CLOSED || door.position == 0.0f),
              "external motion or a closed report cancels the queued toggle");
    }
    {
        Door door(GDO_DOOR_STATE_CLOSING, 0.4f);
        door.set_state(GDO_DOOR_STATE_STOPPED, 0.4f);
        door.set_state(GDO_DOOR_STATE_STOPPED, 0.4f);
        door.set_toggle_only(true);
        door.control(close);
        door.fire("pre_close");
        check(toggles == 1 && door.timers.count("close_door"),
              "duplicate stopped feedback and warning preserve toggle-only reclose direction");
    }
    {
        Door door(GDO_DOOR_STATE_OPENING, 0.4f);
        door.set_state(GDO_DOOR_STATE_STOPPED, 0.4f);
        door.set_toggle_only(true);
        door.control(open);
        command_error = 1;
        door.fire("stop_door");
        command_error = 0;
        door.fire("open_door");
        check(toggles == 1, "failed intermediate Stop cancels the followup toggle");
    }
    for (bool lose_sync : {false, true}) {
        Door door(GDO_DOOR_STATE_OPENING, 0.4f);
        door.set_state(GDO_DOOR_STATE_STOPPED, 0.4f);
        door.set_toggle_only(true);
        door.control(open);
        check(toggles == 1 && door.timers.count("open_door"), "toggle-only reopening queues its followup");
        door.set_state(GDO_DOOR_STATE_CLOSING, 0.3f);
        if (lose_sync) {
            door.set_sync_state(false);
            door.set_sync_state(true);
        } else {
            door.control(stop);
        }
        door.fire("stop_door");
        door.fire("open_door");
        check(toggles == 1, "Stop or sync loss cancels delayed toggle-only commands");
    }
#ifdef USE_MQTT
    {
        Door door(GDO_DOOR_STATE_OPENING, 0.2f);
        door.set_state(GDO_DOOR_STATE_OPENING, 0.3f);
        int before = door.publications;
        door.set_state(GDO_DOOR_STATE_OPENING, 0.4f);
        check(door.publications == before + 1, "MQTT publishes changing position during steady motion");
        door.set_state(GDO_DOOR_STATE_OPENING, 0.4f);
        check(door.publications == before + 1, "MQTT still suppresses identical position and operation");
    }
#endif
    std::cout << failures << " cover behavior failures\n";
    return failures ? 1 : 0;
}

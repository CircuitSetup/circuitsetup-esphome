"""Exercise the real callback/loop methods and ESPHome queue on the host."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import esphome

from dry_contact_runtime import method


ROOT = Path(__file__).resolve().parents[1]
STUBS = r"""
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "esphome/core/lock_free_queue.h"
#define ESP_LOGD(...) assert(!callback_context)
#define ESP_LOGI(...) assert(!callback_context)
#define ESP_LOGW(...) assert(!callback_context)
#define ESP_LOGE(...) assert(!callback_context)
using esp_err_t = int;
using gdo_cb_event_t = int;
using gdo_motor_state_t = int;
constexpr int ESP_OK = 0, GDO_PROTOCOL_SEC_PLUS_V2 = 2, GDO_PROTOCOL_MAX = 4;
constexpr int GDO_CB_EVENT_SYNCED = 0, GDO_CB_EVENT_LIGHT = 1;
constexpr int GDO_CB_EVENT_OPEN_DURATION_MEASUREMENT = 2, GDO_CB_EVENT_CLOSE_DURATION_MEASUREMENT = 3;
constexpr int GDO_CB_EVENT_MOTOR = 4;
constexpr int GDO_DOOR_STATE_UNKNOWN = 0, GDO_MOTOR_STATE_ON = 1, GDO_MOTOR_STATE_OFF = 0,
              GDO_BUTTON_STATE_PRESSED = 1;
struct gdo_paired_device_t { uint8_t total_all = 0; };
struct gdo_status_t {
    int protocol = 2, door = 0, light = 0, lock = 0, motion = 0, obstruction = 0;
    int motor = 0, button = 0, battery = 0, learn = 0;
    gdo_paired_device_t paired_devices;
    bool synced = false;
    uint16_t openings = 0, open_ms = 0, close_ms = 0;
    int32_t door_position = 0;
    uint32_t client_id = 0, rolling_code = 0;
};
gdo_status_t driver;
int status_error = ESP_OK, status_reads = 0;
bool callback_context = false;
std::function<void()> during_status_read;
int gdo_get_status(gdo_status_t *out) {
    ++status_reads; *out = driver;
    if (during_status_read) { auto fn = during_status_read; during_status_read = {}; fn(); }
    return status_error;
}
struct Sensor {
    bool state = false;
    unsigned activations = 0;
    void publish(bool value) { assert(!callback_context); state = value; if (value) ++activations; }
};
using namespace esphome;
"""

COMPONENT = r"""
class GDOComponent {
public:
    gdo_status_t status_;
    bool initialized_ = false, enabled = true, pending_wake = false;
    int defer_calls = 0, sync_events = 0, button_actions = 0;
    bool button_triggered_ = false, cover_triggered_ = false;
    std::vector<std::function<void()>> deferred;
    std::vector<uint32_t> seen;
    std::vector<uint32_t> sync_codes;
    std::function<void()> during_disable;
    std::map<std::string, double> states;
    Sensor button, motor, wireless_remote;
    Sensor *button_sensor_ = &button, *motor_sensor_ = &motor;
    Sensor *wireless_remote_sensor_ = &wireless_remote;
    void setup();
    void loop();
    void defer_gdo_event(const gdo_status_t &, gdo_cb_event_t);
    void handle_gdo_event_(const gdo_status_t &, gdo_cb_event_t);
    bool republish_status_();
    void reset_event_queue_();
    void defer(std::function<void()> fn) { ++defer_calls; deferred.push_back(fn); }
    void enable_loop_soon_any_context() { pending_wake = true; }
    void disable_loop() { if (during_disable) { auto fn = during_disable; during_disable = {}; fn(); } enabled = false; }
    void pump() {
        if (pending_wake) { pending_wake = false; enabled = true; }
        if (enabled) loop();
    }
    int init_driver_() { initialized_ = true; return ESP_OK; }
    void mark_failed() { assert(false); }
    void release_uart_tx_pin_to_safe_state_() {}
    void remember_rolling_code_(uint32_t) {}
    void sync_toggle_only_() {}
    void start_if_ready_() {}
    void set_timeout(const char *, uint32_t, std::function<void()>) {}
    void reset_diagnostic_resync_state() {}
    void set_protocol_state(int value) { states["protocol"] = value; }
    void set_sync_state(bool value) { states["sync"] = value; }
    void set_client_id(uint32_t value) { states["client"] = value; }
    void set_rolling_code(uint32_t value) { states["rolling"] = value; }
    void set_door_state(int value, float pos) { states["door"] = value; states["position"] = pos; }
    void set_light_state(int value) { states["light"] = value; }
    void set_lock_state(int value) { states["lock"] = value; }
    void set_obstruction(int value) { states["obstruction"] = value; }
    void set_motion_state(int value) { states["motion"] = value; }
    void set_battery_state(int value) { states["battery"] = value; }
    void set_learn_state(int value) { states["learn"] = value; }
    void set_openings(uint16_t value) { states["openings"] = value; }
    void set_paired_devices(const gdo_paired_device_t &value) { states["paired"] = value.total_all; }
    void set_open_duration(uint16_t value) { states["open_ms"] = value; }
    void set_close_duration(uint16_t value) { states["close_ms"] = value; }
    void set_button_state(int) { ++button_actions; }
    void set_motor_state(gdo_motor_state_t state);
"""

PROCESS = r"""
void process_gdo_event(const gdo_status_t *status, gdo_cb_event_t event, GDOComponent *gdo) {
    assert(!callback_context);
    gdo->seen.push_back(status->rolling_code);
    if (event == GDO_CB_EVENT_SYNCED) { ++gdo->sync_events; gdo->sync_codes.push_back(status->rolling_code); }
}
void enqueue(GDOComponent &gdo, gdo_status_t &status, int event = GDO_CB_EVENT_LIGHT) {
    callback_context = true;
    gdo_event_handler(&status, event, &gdo);
    callback_context = false;
}
"""

CASES = r"""
int main(int argc, char **argv) {
    assert(argc == 2);
    GDOComponent gdo;
    gdo.setup();
    gdo.pump();
    gdo.defer_calls = 0;
    const std::string scenario = argv[1];
    if (scenario == "snapshots") {
        auto status = driver;
        callback_context = true;
        gdo_event_handler(nullptr, GDO_CB_EVENT_LIGHT, &gdo);
        gdo_event_handler(&status, GDO_CB_EVENT_LIGHT, nullptr);
        callback_context = false;
        for (int i = 1; i <= 3; ++i) { status.rolling_code = i; enqueue(gdo, status); }
        status.rolling_code = 999;
        assert(gdo.seen.empty());
        assert(gdo.defer_calls == 0);
        gdo.pump();
        assert((gdo.seen == std::vector<uint32_t>{1, 2, 3}));
        // Released slots must be reusable, rather than exhausting a lifetime quota.
        for (int i = 0; i < 100; ++i) { enqueue(gdo, status); gdo.pump(); }
        assert(gdo.seen.size() == 103);
    } else if (scenario == "wake") {
        auto status = driver;
        gdo.enabled = true;
        gdo.during_disable = [&]() { status.rolling_code = 42; enqueue(gdo, status); };
        gdo.pump();
        assert(gdo.seen.empty());
        gdo.pump();
        assert((gdo.seen == std::vector<uint32_t>{42}));
    } else {
        auto status = driver;
        for (int i = 0; i < 80; ++i) { status.rolling_code = i; enqueue(gdo, status); }
        if (scenario == "sync" || scenario == "sync_read_error") enqueue(gdo, status, GDO_CB_EVENT_SYNCED);
        driver.synced = scenario != "accepted";
        driver.door = 2; driver.door_position = 10000;
        driver.client_id = 123; driver.rolling_code = 500;
        driver.light = driver.lock = driver.obstruction = driver.motion = 1;
        driver.battery = driver.learn = driver.motor = driver.button = 1;
        driver.openings = 7; driver.paired_devices.total_all = 3;
        if (scenario == "auto_probe") {
            gdo.states["protocol"] = 0;
            driver.protocol = 1; driver.synced = false; driver.door = GDO_DOOR_STATE_UNKNOWN;
        }
        if (scenario.rfind("motor_", 0) == 0) {
            // A cover command or earlier button event normally sets this attribution.
            gdo.button_triggered_ = scenario == "motor_button";
            gdo.cover_triggered_ = scenario != "motor_button";
            if (scenario != "motor_pending") enqueue(gdo, driver, GDO_CB_EVENT_MOTOR);
            driver.motor = GDO_MOTOR_STATE_OFF;
            if (scenario != "motor_pending") enqueue(gdo, driver, GDO_CB_EVENT_MOTOR);
        }
        if (scenario == "calibration" || scenario == "uncalibrated") {
            gdo.states["open_ms"] = 14000; gdo.states["close_ms"] = 17000;
            if (scenario == "calibration") {
                driver.open_ms = 22000; driver.close_ms = 21000;
                enqueue(gdo, driver, GDO_CB_EVENT_OPEN_DURATION_MEASUREMENT);
                enqueue(gdo, driver, GDO_CB_EVENT_CLOSE_DURATION_MEASUREMENT);
            }
        }
        if (scenario == "read_error" || scenario == "sync_read_error" || scenario == "motor_read_error") status_error = 1;
        if (scenario == "sync_race") {
            during_status_read = [&]() {
                driver.rolling_code = 700;
                for (int i = 0; i < 80; ++i) enqueue(gdo, driver);
                enqueue(gdo, driver, GDO_CB_EVENT_SYNCED);
            };
        }
        gdo.pump();
        if (scenario == "auto_probe") {
            assert(gdo.status_.protocol == 1 && gdo.states["sync"] == 0);
            assert(gdo.states["protocol"] == 0);
            driver.protocol = 2; driver.synced = true;
            for (int i = 0; i < 80; ++i) enqueue(gdo, driver);
            gdo.pump();
            assert(gdo.states["protocol"] == 2);
            return 0;
        }
        if (scenario == "sync_race") {
            assert(gdo.sync_events == 0);
            gdo.pump();
            assert((gdo.sync_codes == std::vector<uint32_t>{700}));
            return 0;
        }
        assert(gdo.seen.size() <= 32);
        if (scenario == "read_error" || scenario == "sync_read_error" || scenario == "motor_read_error") {
            assert(gdo.states.empty());
            status_error = ESP_OK;
            gdo.pump();
        }
        if (scenario == "sync" || scenario == "sync_read_error") {
            assert(gdo.sync_events == 1);
            assert(gdo.seen.back() == 500);
        } else {
            assert(gdo.states["sync"] == 1);
            assert(gdo.states["client"] == 123 && gdo.states["rolling"] == 500);
        }
        assert(gdo.states["door"] == 2 && gdo.states["position"] == 0);
        for (const char *name : {"light", "lock", "obstruction", "motion", "battery", "learn"})
            assert(gdo.states[name] == 1);
        assert(gdo.states["paired"] == 3 && gdo.states["openings"] == 7);
        assert(gdo.button.state && gdo.motor.state == (driver.motor == GDO_MOTOR_STATE_ON));
        assert(gdo.button_actions == 0 && gdo.wireless_remote.activations == 0);
        if (scenario.rfind("motor_", 0) == 0) {
            gdo.set_motor_state(GDO_MOTOR_STATE_ON);
            assert(gdo.wireless_remote.activations == (scenario == "motor_pending" ? 0U : 1U));
            assert(!gdo.button_triggered_ && !gdo.cover_triggered_);
        }
        if (scenario == "calibration") {
            assert(gdo.states["open_ms"] == 22000 && gdo.states["close_ms"] == 21000);
        } else if (scenario == "uncalibrated") {
            assert(gdo.states["open_ms"] == 14000 && gdo.states["close_ms"] == 17000);
        }
        const auto reads = status_reads;
        gdo.pump();
        assert(status_reads == reads);
    }
}
"""


@unittest.skipUnless(shutil.which("g++"), "g++ is required for the C++ host checks")
class EventQueueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        source = (ROOT / "components/secplus_gdo/secplus_gdo.cpp").read_text(encoding="utf-8")
        header = (ROOT / "components/secplus_gdo/secplus_gdo.h").read_text(encoding="utf-8")
        declarations = header[header.index("    struct GDOEvent"):header.index("    class GDOComponent")]
        members = header[header.index("        GDOEvent event_slots_"):header.index("        gdo_status_t      status_")]
        harness = STUBS + declarations + COMPONENT + members + "};\n"
        harness += "void process_gdo_event(const gdo_status_t *, gdo_cb_event_t, GDOComponent *);\n"
        harness += method(source, "static void gdo_event_handler(")
        harness += PROCESS
        for name in ("defer_gdo_event", "setup", "handle_gdo_event_", "republish_status_", "loop", "set_motor_state", "reset_event_queue_"):
            harness += method(source, f"GDOComponent::{name}(")
        harness += CASES
        cpp = Path(cls.temp.name) / "events.cpp"
        cpp.write_text(harness, encoding="utf-8")
        cls.executable = Path(cls.temp.name) / "events"
        subprocess.run(["g++", "-std=c++17", "-I", str(Path(esphome.__file__).parent.parent),
                        str(cpp), "-o", str(cls.executable)], check=True)

    def test_callback_copies_snapshots_without_scheduling_or_publishing(self):
        subprocess.run([str(self.executable), "snapshots"], check=True)

    def test_event_during_loop_disable_is_not_stranded(self):
        subprocess.run([str(self.executable), "wake"], check=True)

    def test_overflow_refreshes_latched_states_without_replaying_actions(self):
        subprocess.run([str(self.executable), "overflow"], check=True)

    def test_overflow_preserves_accepted_rolling_code_sync(self):
        subprocess.run([str(self.executable), "accepted"], check=True)

    def test_dropped_sync_event_still_runs_sync_recovery(self):
        subprocess.run([str(self.executable), "sync"], check=True)

    def test_failed_status_read_retries_refresh(self):
        subprocess.run([str(self.executable), "read_error"], check=True)

    def test_failed_status_read_preserves_dropped_sync(self):
        subprocess.run([str(self.executable), "sync_read_error"], check=True)

    def test_new_sync_drop_during_refresh_uses_a_new_snapshot(self):
        subprocess.run([str(self.executable), "sync_race"], check=True)

    def test_overflow_recovers_learned_travel_calibration(self):
        subprocess.run([str(self.executable), "calibration"], check=True)

    def test_unknown_driver_calibration_does_not_clear_saved_durations(self):
        subprocess.run([str(self.executable), "uncalibrated"], check=True)

    def test_dropped_motor_on_does_not_hide_the_next_remote_activation(self):
        for scenario in ("motor_button", "motor_cover", "motor_read_error"):
            with self.subTest(scenario=scenario):
                subprocess.run([str(self.executable), scenario], check=True)

    def test_unrelated_overflow_retains_attribution_for_a_pending_command(self):
        subprocess.run([str(self.executable), "motor_pending"], check=True)

    def test_overflow_does_not_persist_a_tentative_auto_protocol(self):
        subprocess.run([str(self.executable), "auto_probe"], check=True)


if __name__ == "__main__":
    unittest.main()

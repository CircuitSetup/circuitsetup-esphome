"""Run the component's diagnostic restart against gdolib's reset contract."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]

STUBS = r"""
#include <cassert>
#include <cstdint>
#include <functional>
#include <inttypes.h>
#include <string>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
constexpr int ESP_OK = 0, GDO_PROTOCOL_SEC_PLUS_V2 = 2;
constexpr int GDO_PROTOCOL_MAX = 4;
using gdo_cb_event_t = int;
constexpr int GDO_DOOR_STATE_OPEN = 1, GDO_DOOR_STATE_OPENING = 4,
              GDO_DOOR_STATE_CLOSING = 5, GDO_MOTOR_STATE_OFF = 0,
              GDO_MOTOR_STATE_ON = 1;
constexpr uint8_t MAX_DIAGNOSTIC_DRIVER_RESTARTS = 3;
struct gdo_status_t {
    int protocol = GDO_PROTOCOL_SEC_PLUS_V2;
    uint32_t client_id = 10504, rolling_code = 100;
    uint16_t open_ms = 14000, close_ms = 17000;
    int door = GDO_DOOR_STATE_OPEN, motor = GDO_MOTOR_STATE_OFF;
};
gdo_status_t driver;
unsigned deinitializations = 0, duration_writes = 0;
int gdo_get_status(gdo_status_t *out) { *out = driver; return ESP_OK; }
int gdo_deinit() {
    ++deinitializations;
    driver = {};
    // gdolib deinit clears the calibration as well as driver state.
    driver.open_ms = driver.close_ms = 0;
    return ESP_OK;
}
int gdo_set_protocol(int value) { driver.protocol = value; return ESP_OK; }
int gdo_set_client_id(uint32_t value) { driver.client_id = value; return ESP_OK; }
int gdo_set_rolling_code(uint32_t value) { driver.rolling_code = value; return ESP_OK; }
int gdo_set_open_duration(uint16_t value) {
    ++duration_writes;
    assert(value >= 1000 && value <= 65000);
    driver.open_ms = value;
    return ESP_OK;
}
int gdo_set_close_duration(uint16_t value) {
    ++duration_writes;
    assert(value >= 1000 && value <= 65000);
    driver.close_ms = value;
    return ESP_OK;
}
class GDOComponent {
 public:
    bool diagnostic_driver_restart_pending_ = false, initialized_ = true, started_ = true;
    uint8_t diagnostic_driver_restart_attempt_count_ = 0;
    std::function<void()> timeout;
    gdo_status_t status_;
    GDOComponent() { status_.protocol = 0; }
    void defer_gdo_event(const gdo_status_t &, gdo_cb_event_t);
    void defer(std::function<void()> fn) { fn(); }
    void schedule_diagnostic_driver_restart_();
    void restart_driver_for_diagnostic_sync_();
    void set_timeout(const char *, uint32_t, std::function<void()> fn) { timeout = fn; }
    void release_uart_tx_pin_to_safe_state_() {}
    int init_driver_() { initialized_ = true; return ESP_OK; }
    void mark_failed() { assert(false); }
    void remember_rolling_code_(uint32_t) {}
    void sync_toggle_only_() {}
    void start_if_ready_() { started_ = true; }
};
void process_gdo_event(const gdo_status_t *, gdo_cb_event_t, GDOComponent *) {}
"""

CASES = r"""
int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    GDOComponent component;
    if (scenario == "durations") {
        component.restart_driver_for_diagnostic_sync_();
        assert(driver.open_ms == 14000 && driver.close_ms == 17000);
        assert(driver.client_id == 10504 && driver.rolling_code == 100);
        assert(deinitializations == 1 && duration_writes == 2);
    } else if (scenario == "uncalibrated") {
        driver.open_ms = driver.close_ms = 0;
        component.restart_driver_for_diagnostic_sync_();
        assert(deinitializations == 1 && duration_writes == 0);
    } else if (scenario == "auto_v2" || scenario == "auto_v1" || scenario == "auto_unknown") {
        if (scenario != "auto_unknown") {
            driver.protocol = scenario == "auto_v2" ? GDO_PROTOCOL_SEC_PLUS_V2 : 1;
            component.defer_gdo_event(driver, 0);
        }
        // A failed auto-detection sync resets the library protocol to zero.
        driver.protocol = 0;
        component.defer_gdo_event(driver, 0);
        component.restart_driver_for_diagnostic_sync_();
        assert(deinitializations == (scenario == "auto_v2" ? 1U : 0U));
    } else {
        if (scenario == "opening") driver.door = GDO_DOOR_STATE_OPENING;
        if (scenario == "closing") driver.door = GDO_DOOR_STATE_CLOSING;
        if (scenario == "motor") driver.motor = GDO_MOTOR_STATE_ON;
        component.restart_driver_for_diagnostic_sync_();
        // Restarting during a move drops gdolib's pending position STOP.
        assert(deinitializations == 0);
        assert(component.diagnostic_driver_restart_attempt_count_ == 0);
        assert(component.diagnostic_driver_restart_pending_ && component.timeout);
        driver.door = GDO_DOOR_STATE_OPEN;
        driver.motor = GDO_MOTOR_STATE_OFF;
        auto timeout = component.timeout;
        timeout();
        assert(deinitializations == 1);
        assert(component.diagnostic_driver_restart_attempt_count_ == 1);
        assert(driver.open_ms == 14000 && driver.close_ms == 17000);
    }
}
"""


@unittest.skipUnless(shutil.which("g++"), "g++ is required for the C++ host checks")
class DiagnosticRestartTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        source = (ROOT / "components/secplus_gdo/secplus_gdo.cpp").read_text(encoding="utf-8")
        start = source.index("    void GDOComponent::schedule_diagnostic_driver_restart_()")
        end = source.index("    void GDOComponent::reset_diagnostic_resync_state()", start)
        callback_start = source.index("    void GDOComponent::defer_gdo_event(")
        callback_end = source.index("    void GDOComponent::start_gdo()", callback_start)
        harness = Path(cls.temp.name) / "restart.cpp"
        harness.write_text(STUBS + source[callback_start:callback_end] + source[start:end] + CASES, encoding="utf-8")
        cls.executable = Path(cls.temp.name) / "restart"
        subprocess.run(["g++", "-std=c++14", str(harness), "-o", str(cls.executable)], check=True)

    def test_restart_preserves_calibration(self):
        subprocess.run([str(self.executable), "durations"], check=True)

    def test_restart_keeps_unknown_calibration(self):
        subprocess.run([str(self.executable), "uncalibrated"], check=True)

    def test_restart_waits_for_motion_to_stop(self):
        for state in ("opening", "closing", "motor"):
            with self.subTest(state=state):
                subprocess.run([str(self.executable), state], check=True)

    def test_restart_recovers_auto_v2_without_changing_v1(self):
        for protocol in ("auto_v2", "auto_v1", "auto_unknown"):
            with self.subTest(protocol=protocol):
                subprocess.run([str(self.executable), protocol], check=True)


if __name__ == "__main__":
    unittest.main()

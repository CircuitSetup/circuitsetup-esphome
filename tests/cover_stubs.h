#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
#define ESP_LOGCONFIG(...) ((void)0)
using esp_err_t = int;
constexpr int ESP_OK = 0;
enum gdo_door_state_t { GDO_DOOR_STATE_UNKNOWN, GDO_DOOR_STATE_OPEN,
    GDO_DOOR_STATE_CLOSED, GDO_DOOR_STATE_STOPPED, GDO_DOOR_STATE_OPENING,
    GDO_DOOR_STATE_CLOSING, GDO_DOOR_STATE_MAX };
struct gdo_status_t { gdo_door_state_t door; int32_t door_position; };
gdo_status_t status{GDO_DOOR_STATE_OPEN, 0};
int opens = 0, stops = 0, toggles = 0, closes = 0, command_error = 0, status_error = 0, status_reads = 0;
const char *esp_err_to_name(int) { return "test error"; }
const char *gdo_door_state_to_string(gdo_door_state_t) { return "test state"; }
int gdo_door_toggle() { ++toggles; return command_error; }
int gdo_door_close() { ++closes; return command_error; }
int gdo_door_open() { ++opens; return command_error; }
int gdo_door_stop() { ++stops; return command_error; }
int gdo_door_move_to_target(uint32_t) { return command_error; }
int gdo_get_status(gdo_status_t *out) { ++status_reads; *out = status; return status_error; }
namespace esphome {
template<typename... T> class Trigger { public: void trigger(T...) {} };
class Component {
 public:
    virtual void dump_config() {}
    std::map<std::string, std::pair<uint32_t, std::function<void()>>> timers;
    void set_timeout(const std::string &name, uint32_t ms, std::function<void()> fn) {
        timers[name] = {ms, std::move(fn)};
    }
    bool cancel_timeout(const std::string &name) { return timers.erase(name) != 0; }
    bool fire(const std::string &name) {
        auto it = timers.find(name);
        if (it == timers.end()) return false;
        auto fn = std::move(it->second.second);
        timers.erase(it);
        fn();
        return true;
    }
};
namespace cover {
constexpr float COVER_OPEN = 1.0f, COVER_CLOSED = 0.0f;
enum CoverOperation { COVER_OPERATION_IDLE, COVER_OPERATION_OPENING, COVER_OPERATION_CLOSING };
class CoverTraits { public:
    void set_supports_stop(bool) {} void set_supports_toggle(bool) {}
    void set_supports_position(bool) {}
};
class CoverCall { public:
    bool toggle = false, stop = false;
    std::optional<float> target;
    bool get_toggle() const { return toggle; }
    bool get_stop() const { return stop; }
    std::optional<float> get_position() const { return target; }
};
class Cover { public:
    virtual CoverTraits get_traits() = 0;
    virtual void control(const CoverCall &) = 0;
    float position = COVER_OPEN;
    CoverOperation current_operation = COVER_OPERATION_IDLE;
    int publications = 0;
    void publish_state(bool) { ++publications; }
};
}
namespace secplus_gdo { class GDOComponent { public: void notify_cover_command() {} }; }
}

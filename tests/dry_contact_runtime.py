"""Host interfaces for executing the installed ESPHome GPIO/filter/button methods."""


def method(source, name):
    start = source.index(name)
    start = source.rfind("\n", 0, start) + 1
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def without_includes(source):
    return "\n".join(line for line in source.splitlines() if not line.startswith(("#include", "#pragma once")))


def callback(actions):
    """Translate the auxiliary blink callback; cover commands have a separate harness."""
    result = ""
    for action in actions:
        if "if" in action:
            branch = action["if"]
            condition = branch["condition"]["lambda"]
            condition = getattr(condition, "value", str(condition))
            condition = condition.replace("id(garage_door).position", "door_position")
            result += f"if ([&]() -> bool {{ {condition} }}()) {{\n"
            result += callback(branch.get("then", [])) + "} else {\n"
            result += callback(branch.get("else", [])) + "}\n"
        elif "script.execute" in action:
            script_id = str(action["script.execute"]["id"])
            assert script_id in ("blink_status_led", "error_tone_1")
            result += "++blinks;\n" if script_id == "blink_status_led" else "++errors;\n"
        elif "button.press" in action:
            assert str(action["button.press"]["id"]) == "garage_door_opener_button"
            result += "button.press();\n"
        elif "cover.template.publish" in action:
            publish = action["cover.template.publish"]
            assert str(publish["id"]) == "garage_door"
            if "state" in publish:
                result += f"door_position = {'1.0f' if publish['state'] == 'OPEN' else '0.0f'};\n"
            if "current_operation" in publish:
                result += f"operation = {1 if 'OPENING' in str(publish['current_operation']) else 0};\n"
        elif "lambda" in action:
            # Only the error branch's log action has no GPIO or cover side effects.
            assert "ESP_LOGW" in str(action["lambda"])
        else:
            raise AssertionError(f"Unhandled sensor action: {action}")
    return result


STUBS = r"""
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#define ESP_LOGCONFIG(...) ((void)0)
#define LOG_BUTTON(...) ((void)0)
#define LOG_PIN(...) ((void)0)
#define id(x) x
namespace esphome {
template<class T> using optional = std::optional<T>;
uint32_t now = 0;
using TimerKey = std::pair<const void *, std::string>;
std::map<TimerKey, std::pair<uint32_t, std::function<void()>>> timers;
struct Scheduler {
  void set_timeout(const void *owner, uint32_t delay, std::function<void()> fn) { timers[{owner,"filter"}] = {now+delay,fn}; }
};
struct Application { Scheduler scheduler; } App;
void advance(uint32_t elapsed) {
  const auto target = now + elapsed;
  for (;;) {
    auto next = timers.end();
    for (auto it = timers.begin(); it != timers.end(); ++it)
      if (it->second.first <= target && (next == timers.end() || it->second.first < next->second.first)) next = it;
    if (next == timers.end()) break;
    now = next->second.first;
    auto fn = next->second.second; timers.erase(next); fn();
  }
  now = target;
}
namespace setup_priority { constexpr float HARDWARE = 800; }
class Component {
 public:
  virtual void setup() {} virtual void dump_config() {} virtual float get_setup_priority() const { return 0; }
  void disable_loop() {}
  void set_timeout(const char *key, uint32_t delay, std::function<void()> fn) { timers[{this,key}] = {now+delay, fn}; }
  void set_timeout(uint32_t key, uint32_t delay, std::function<void()> fn) { set_timeout(std::to_string(key).c_str(), delay, fn); }
};
struct GPIOPin {
  int number; bool value = false; unsigned high_writes = 0;
  explicit GPIOPin(int number) : number(number) {}
  void setup() {} bool digital_read() { return value; }
  void digital_write(bool state) { value = state; if (state) ++high_writes; }
};
namespace button { class Button { public: void press() { press_action(); } protected: virtual void press_action() = 0; }; }
namespace binary_sensor {
constexpr uint32_t FILTER_TIMEOUT_ID = 0;
struct Dedup { optional<bool> last; bool next(bool value) { if (last == value) return false; last = value; return true; } };
struct BinarySensor { std::function<void(bool)> callback; void send_state_internal(bool value) { callback(value); } };
class Filter {
 public:
  Filter *next_ = nullptr; BinarySensor *parent_ = nullptr; Dedup dedup_;
  virtual optional<bool> new_value(bool value) = 0;
  void input(bool value); void output(bool value);
};
struct Delay { uint32_t delay; uint32_t value() const { return delay; } };
class DelayedOnOffFilter : public Filter, public Component {
 public:
  Delay on_delay_{200}, off_delay_{200};
  optional<bool> new_value(bool value) override;
};
}
namespace gpio {
struct Store { bool use_interrupt_ = false; bool is_changed() { return false; } void clear_changed() {} bool get_state() { return false; } };
class GPIOBinarySensor : public Component {
 public:
  GPIOPin *pin_; bool use_interrupt_ = false; Store store_; binary_sensor::Filter *filter;
  void publish_state(bool value) { filter->input(value); }
  void loop();
};
}
}
"""


def build_harness(config, esphome_root):
    sensor = next(item for item in config["binary_sensor"] if str(item.get("id")) == "garage_door_input")
    spare = next(item for item in config["binary_sensor"] if item["name"] == "Spare Aux Input")
    output = next(item for item in config["output"] if str(item.get("id")) == "garage_door_opener_output")
    button = next(item for item in config["button"] if str(item.get("id")) == "garage_door_opener_button")
    components = esphome_root / "components"
    result = STUBS
    for filename in ("output/binary_output.h", "gpio/output/gpio_binary_output.h", "output/button/output_button.h"):
        result += without_includes((components / filename).read_text(encoding="utf-8")) + "\n"
    result += "namespace esphome::output {\n" + method((components / "output/button/output_button.cpp").read_text(), "OutputButton::press_action()")
    result += "void OutputButton::dump_config() {}\n}\n"
    filter_source = (components / "binary_sensor/filter.cpp").read_text()
    result += "namespace esphome::binary_sensor {\n"
    for name in ("Filter::output(bool", "Filter::input(bool", "DelayedOnOffFilter::new_value(bool"):
        result += method(filter_source, name)
    result += "}\nnamespace esphome::gpio {\n"
    result += method((components / "gpio/binary_sensor/gpio_binary_sensor.cpp").read_text(), "GPIOBinarySensor::loop()")
    result += "void GPIOBinaryOutput::dump_config() {}\n}\n"
    result += "using namespace esphome;\nint main() {\nbool blink_on_state=true, spare_state=false; unsigned blinks=0, errors=0; float door_position=-1.0f;\n"
    result += f"GPIOPin input({sensor['pin']['number']}), spare_input({spare['pin']['number']}), relay({output['pin']['number']});\n"
    result += "gpio::GPIOBinaryOutput output; output.set_pin(&relay); output.setup(); assert(!relay.value);\n"
    result += "output::OutputButton button; button.set_output(&output);\n"
    result += f"button.set_duration({button['duration'].total_milliseconds});\n"
    for name, item, pin in (("door", sensor, "input"), ("spare", spare, "spare_input")):
        delay = item["filters"][0]["delayed_on_off"].total_milliseconds
        result += f"binary_sensor::BinarySensor {name}_published; binary_sensor::DelayedOnOffFilter {name}_filter;\n"
        result += f"{name}_filter.parent_=&{name}_published; {name}_filter.on_delay_.delay={delay}; {name}_filter.off_delay_.delay={delay};\n"
        result += f"gpio::GPIOBinarySensor {name}_sensor; {name}_sensor.pin_=&{pin}; {name}_sensor.filter=&{name}_filter;\n"
        result += f"{name}_published.callback=[&](bool x) {{\n"
        if name == "spare":
            result += "spare_state=x;\n"
        else:
            result += "door_position=x ? 1.0f : 0.0f;\n"
        # Isolate the GPIO blink callback; command tests execute the cover extension.
        blink_actions = [action for automation in item["on_state"] for action in automation["then"]
                         if "id(blink_on_state)" in str(action.get("if", {}).get("condition", {}).get("lambda", ""))]
        assert len(blink_actions) == 1
        result += callback(blink_actions)
        result += "};\n"
    result += r"""
  door_sensor.loop(); advance(199); assert(door_position == -1.0f);
  advance(1); assert(door_position == 0.0f);
  input.value = true; door_sensor.loop(); advance(100);
  input.value = false; door_sensor.loop(); advance(200);
  assert(door_position == 0.0f && relay.high_writes == 0);
  input.value = true; door_sensor.loop(); advance(199); assert(door_position == 0.0f);
  advance(1); assert(door_position == 1.0f && relay.high_writes == 0);
  auto before_blinks = blinks;
  spare_input.value = true; spare_sensor.loop(); advance(200);
  assert(spare_state && blinks == before_blinks+1 && door_position == 1.0f && relay.high_writes == 0);
  blink_on_state = false; spare_input.value = false; spare_sensor.loop(); advance(200);
  assert(!spare_state && blinks == before_blinks+1 && door_position == 1.0f && relay.high_writes == 0);
  blink_on_state = true;
  button.press(); assert(relay.value); advance(299); assert(relay.value);
  advance(1); assert(!relay.value && relay.high_writes == 1);
  button.press(); advance(200); button.press(); advance(299); assert(relay.value);
  advance(1); assert(!relay.value && relay.high_writes == 3);
  input.value = false; door_sensor.loop(); advance(199); assert(door_position == 1.0f);
  advance(1); assert(door_position == 0.0f && relay.high_writes == 3);
}
"""
    return result

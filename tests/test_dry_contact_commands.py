"""Dry-contact regression harness: actual ESPHome automation classes, YAML actions.

Hardware, cover publication, and clock are substituted. Run with ESPHome 2026.8.1.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

import esphome
from esphome.yaml_util import load_yaml

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / "tests"))
from dry_contact_runtime import method

EP = Path(esphome.__file__).parent
automation = (EP / "core/automation.h").read_text()
base = (EP / "core/base_automation.h").read_text()
cover_source = (EP / "components/template/cover/template_cover.cpp").read_text()
cover_package = load_yaml(ROOT / "packages/garage-door-cover.yaml")
cover = cover_package["cover"][0]
wired = load_yaml(ROOT / "packages/garage-door-cover-wired.yaml")["binary_sensor"][0]["on_state"]
warning_package = load_yaml(ROOT / "packages/pre-close-warning-tones.yaml")
warning = warning_package["button"][0]["on_press"]
scripts = cover_package.get("script", []) + warning_package.get("script", [])
script_source = (EP / "components/script/script.h").read_text()


def cpp_class(name):
    start = base.index("template", base.rfind("\n", 0, base.index("class " + name)))
    return method(base[start:], "class " + name) + ";\n"


source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#define ESPHOME_ALWAYS_INLINE
#define ESP_LOGW(...) ((void)0)
#define ESPHOME_LOG_FORMAT(x) x
#define LOG_STR_ARG(x) x
#define id(x) x
namespace esphome {
uint32_t now = 0;
uint32_t millis() { return now; }
using std::clamp;
std::function<void()> loop_hook;
struct Component {
  static inline std::vector<Component *> all;
  bool enabled=true;
  Component() { all.push_back(this); }
  virtual void setup() {} virtual void loop() {}
  void enable_loop() { enabled=true; } void disable_loop() { enabled=false; }
};
using LogString=char;
struct ScopedSourceGuard { explicit ScopedSourceGuard(const char *) {} };
struct Scheduler {
  struct SchedulerItem { enum { TIMEOUT }; };
  enum class NameType { SELF_POINTER };
  struct Timer { const void *key; uint32_t due; std::function<void()> fn; };
  std::vector<Timer> timers;
  void cancel_timeout(const void *key) {
    std::erase_if(timers, [=](const auto &t) { return t.key == key; });
  }
  void set_timer_common_(void *, int, NameType, const char *key, int, uint32_t delay,
                         std::function<void()> fn, bool skip_cancel, void *) {
    if (!skip_cancel) cancel_timeout(key);
    timers.push_back({key, now + delay, fn});
  }
  void advance(uint32_t ms) {
    for(uint32_t elapsed=0; elapsed<ms; ++elapsed) {
      ++now;
      if(loop_hook) loop_hook();
      for (;;) {
        auto it=std::find_if(timers.begin(),timers.end(),[](auto &t){ return int32_t(now-t.due)>=0; });
        if(it==timers.end()) break;
        auto t=*it; timers.erase(it); t.fn();
      }
      for(auto *c : Component::all) if(c->enabled) c->loop();
      if(loop_hook) loop_hook();
    }
  }
};
struct Application { Scheduler scheduler; void *get_current_source() { return nullptr; } } App;
template<class T, class... Ts> struct Value { std::function<T(Ts...)> f; T value(Ts... args) { return f(args...); } };
#define TEMPLATABLE_VALUE(type, name) Value<type, Ts...> name##_; template<class F> void set_##name(F f) { name##_.f=f; }
'''
source += automation[automation.index("template<typename... Ts> class Condition") :]
source += "namespace esphome {\n"
for name in ("LambdaCondition", "NotCondition", "DelayAction", "LambdaAction", "ContinuationAction"):
    source += cpp_class(name)
start = base.index("template<bool Enabled, typename... Ts> struct OptionalContinuation")
end = base.index("template<typename... Ts> class WhileAction :", start)
source += base[start:end]
source += 'namespace script { struct ScriptLogger { void esp_logw_(int,const char*,const char*) {} void esp_logd_(int,const char*,const char*) {} };\n'
for name in ("Script", "SingleScript", "RestartScript", "ScriptWaitAction"):
    source += method(script_source, "class " + name + (" :" if name != "ScriptWaitAction" else " final")) + ";\n"
source += '}\n'
source += r'''
namespace cover {
constexpr float COVER_OPEN=1.0f, COVER_CLOSED=0.0f;
enum CoverOperation { COVER_OPERATION_IDLE, COVER_OPERATION_OPENING, COVER_OPERATION_CLOSING };
struct CoverCall {
  bool stop=false; std::optional<bool> toggle; std::optional<float> position, tilt;
  std::function<void(const CoverCall &)> perform_callback;
  void set_command_open() { position=COVER_OPEN; }
  void set_command_close() { position=COVER_CLOSED; }
  void perform() { perform_callback(*this); }
  bool get_stop() const { return stop; }
  auto get_toggle() const { return toggle; }
  auto get_position() const { return position; }
  auto get_tilt() const { return tilt; }
};
}
namespace template_ {
using namespace cover;
struct TemplateCover {
  float position=COVER_OPEN, tilt=0;
  CoverOperation current_operation=COVER_OPERATION_IDLE;
  Trigger<> open_trigger_, close_trigger_, stop_trigger_, toggle_trigger_;
  Trigger<float> position_trigger_, tilt_trigger_;
  Trigger<> *prev_command_trigger_=nullptr;
  bool optimistic_=false;
  CoverCall make_call() { CoverCall c; c.perform_callback=[this](auto &call) { control(call); }; return c; }
  void publish_state() {}
  void control(const CoverCall &call);
  void stop_prev_trigger_();
};
'''
source += method(cover_source, "TemplateCover::control(")
source += method(cover_source, "TemplateCover::stop_prev_trigger_()")
source += r'''
}
}
using namespace esphome;
using namespace esphome::cover;
struct Fixture {
  template_::TemplateCover garage_door;
  struct Sensor { bool state=true; } garage_door_input;
  bool sensor_open=true, x=false, warning_led=false, sound=false;
  uint32_t open_ms=14000, close_ms=12000;
  std::vector<uint32_t> pulses;
  Trigger<> warning_trigger, sensor_trigger;
  void command(const char *name) {
    CoverCall call;
    std::string s=name;
    if(s=="OPEN") call.position=COVER_OPEN;
    if(s=="CLOSE") call.position=COVER_CLOSED;
    if(s=="STOP") call.stop=true;
    if(s=="TOGGLE") call.toggle=true;
    garage_door.control(call);
  }
  void sensor(bool value) { garage_door_input.state=sensor_open=x=value; sensor_trigger.trigger(); refresh(); }
  void refresh();
  Fixture();
'''
for item in cover_package.get('globals', []):
    source += f"  {item['type']} {item['id']}={item['initial_value']};\n"
for item in scripts:
    kind = 'RestartScript' if item.get('mode') == 'restart' else 'SingleScript'
    types = ','.join(item.get('parameters', {}).values())
    source += f"  script::{kind}<{types}> {item['id']};\n"
source += '};\nvoid Fixture::refresh() {\n'


def expression(value):
    value = str(value)
    value = value.replace('${garage_door_open_duration_ms}', 'open_ms')
    value = value.replace('${garage_door_close_duration_ms}', 'close_ms')
    assert '${' not in value, value
    return value


if 'lambda' in cover:
    source += 'auto value=[this]() -> std::optional<float> {' + expression(cover['lambda']) + ' }(); if(value) garage_door.position=*value;\n'
source += '}\nFixture::Fixture() { loop_hook=[this]() { refresh(); };\n'
counter = 0


def cond(item, params):
    types = ','.join(params.values())
    arguments = ','.join(f'{kind} {name}' for name, kind in params.items())
    if "lambda" in item:
        return f"new LambdaCondition<{types}>([this]({arguments}) -> bool {{ " + expression(item["lambda"]) + " })"
    if "not" in item:
        return f"new NotCondition<{types}>(" + cond(item["not"], params) + ")"
    assert list(item) == ["binary_sensor.is_on"], item
    return "new LambdaCondition<>([this]() { return sensor_open; })"


def actions(items, params=None):
    global counter, source
    params = params or {}
    types = ','.join(params.values())
    arguments = ','.join(f'{kind} {name}' for name, kind in params.items())
    names = []
    for item in items:
        counter += 1
        name = f"action_{counter}"
        names.append(name)
        if "if" in item:
            branch = item["if"]
            source += f"auto *{name} = new IfAction<true{',' if types else ''}{types}>({cond(branch['condition'], params)});\n"
            then = actions(branch.get("then", []), params)
            other = actions(branch.get("else", []), params)
            source += f"{name}->add_then({{{','.join(then)}}});\n{name}->add_else({{{','.join(other)}}});\n"
        elif "delay" in item:
            delay = item['delay']
            if hasattr(delay, 'value'):
                body = expression(delay)
            else:
                delay = {'$garage_door_close_warning_duration': '5000ms', '$garage_door_opener_momentary_duration': '300ms'}.get(str(delay), str(delay))
                assert delay.endswith('ms'), delay
                body = f'return {int(delay[:-2])};'
            source += f"auto *{name}=new DelayAction<{types}>(); {name}->set_delay([this]({arguments}) -> uint32_t {{ {body} }});\n"
        elif 'script.wait' in item:
            target = item['script.wait']
            source += f'auto *{name}=new script::ScriptWaitAction<decltype({target}){"," if types else ""}{types}>(&{target});\n'
        else:
            if "button.press" in item:
                target = item["button.press"]
                effect = {"garage_door_opener_button": "pulses.push_back(now);", "pre_close_warning": "warning_trigger.trigger();", "warning_tone": "sound=true;"}[target]
            elif "cover.template.publish" in item:
                values = item["cover.template.publish"]
                effect = ""
                if "state" in values:
                    effect += "garage_door.position=COVER_" + values["state"] + ";"
                if "current_operation" in values:
                    effect += "garage_door.current_operation=COVER_OPERATION_" + values["current_operation"] + ";"
            elif "light.turn_on" in item:
                effect = "warning_led=true;"
            elif "light.turn_off" in item:
                effect = "warning_led=false;"
            elif "rtttl.stop" in item:
                effect = "sound=false;"
            elif "script.execute" in item:
                target = item['script.execute']
                if isinstance(target, dict):
                    target_id = target['id']
                    values = ','.join(str(value).lower() for key,value in target.items() if key != 'id')
                else:
                    target_id, values = target, ''
                effect = f'{target_id}.execute({values});'
            elif 'script.stop' in item:
                effect = f"{item['script.stop']}.stop();"
            elif "lambda" in item:
                effect = expression(item['lambda'])
            else:
                raise AssertionError(item)
            source += f"auto *{name}=new LambdaAction<{types}>([this]({arguments}) {{ {effect} }});\n"
    return names


for trigger, items in [("garage_door." + cmd + "_trigger_", cover[cmd + "_action"]) for cmd in ("open", "close", "stop", "toggle")] + [("warning_trigger", warning), ("sensor_trigger", wired)]:
    names = actions(items)
    source += f"(new Automation<>(&{trigger}))->add_actions({{{','.join(names)}}});\n"
for script in scripts:
    params = script.get('parameters', {})
    names = actions(script['then'], params)
    source += f"(new Automation<{','.join(params.values())}>(&{script['id']}))->add_actions({{{','.join(names)}}});\n"
source += r'''
}
void reset_clock() { App.scheduler.timers.clear(); Component::all.clear(); loop_hook={}; now=0; }
int main(int argc, char **argv) {
  const int scenario = std::stoi(argv[1]);
  if (scenario == 1) {
    reset_clock(); Fixture f; f.sensor(false); f.command("OPEN");
    assert(f.pulses.size()==1 && f.garage_door.current_operation==COVER_OPERATION_OPENING);
    App.scheduler.advance(1000); f.sensor(true); // reed has cleared and debounced
    f.command("STOP");
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE && f.pulses.size()==2);
    std::cout << "PASS: Stop after opening leaves the closed reed\n";
  }
  if (scenario == 2) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(5000);
    assert(f.pulses.size()==1 && f.garage_door.current_operation==COVER_OPERATION_CLOSING);
    App.scheduler.advance(1000); f.command("CLOSE"); App.scheduler.advance(5000);
    assert((f.pulses==std::vector<uint32_t>{5000}));
    assert(f.garage_door.current_operation==COVER_OPERATION_CLOSING);
    std::cout << "PASS: duplicate Close while closing adds no relay pulse\n";
  }
  if (scenario == 3) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(2000); f.command("STOP"); f.command("CLOSE");
    assert(f.warning_led && f.sound);
    App.scheduler.advance(3000);
    assert(f.warning_led && f.sound && f.pulses.empty());
    App.scheduler.advance(1999); assert(f.pulses.empty());
    App.scheduler.advance(1); assert((f.pulses==std::vector<uint32_t>{7000}));
    std::cout << "PASS: cancelled warning cannot silence a newer warning\n";
  }
  if (scenario == 4) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(2000); f.command("STOP");
    assert(!f.warning_led && !f.sound);
    App.scheduler.advance(10000); assert(f.pulses.empty());
    std::cout << "PASS: Stop cancels warning outputs and relay\n";
  }
  if (scenario == 5) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(2000); f.sensor(false); App.scheduler.advance(5000);
    assert(f.pulses.empty());
    std::cout << "PASS: reed closes during warning -> no relay pulse\n";
  }
  if (scenario == 6) {
    reset_clock(); Fixture f; f.sensor(false); f.command("OPEN");
    App.scheduler.advance(1000); f.sensor(true);
    App.scheduler.advance(12999);
    assert(f.garage_door.current_operation==COVER_OPERATION_OPENING);
    App.scheduler.advance(1);
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE && f.garage_door.position==1.0f);
    f.command("STOP"); assert(f.pulses.size()==1);
    std::cout << "PASS: calibrated opening finishes without a stop pulse\n";
  }
  if (scenario == 7) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(2000); f.command("CLOSE");
    App.scheduler.advance(3000);
    assert((f.pulses==std::vector<uint32_t>{5000}));
    assert(!f.warning_led && !f.sound);
    std::cout << "PASS: duplicate Close preserves the original warning countdown\n";
  }
  if (scenario == 8) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(1000); f.command("OPEN");
    assert(!f.warning_led && !f.sound);
    App.scheduler.advance(10000); assert(f.pulses.empty());
    std::cout << "PASS: Open cancels pending Close without moving an open door\n";
  }
  if (scenario == 9) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(6000); f.command("TOGGLE");
    assert((f.pulses==std::vector<uint32_t>{5000,6000}));
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE);
    App.scheduler.advance(20000); assert(f.pulses.size()==2);
    std::cout << "PASS: Toggle immediately stops motion and cancels completion timer\n";
  }
  if (scenario == 10) {
    reset_clock(); Fixture f; f.sensor(false); f.command("OPEN");
    App.scheduler.advance(100); f.command("STOP");
    App.scheduler.advance(299); assert(f.pulses.size()==1);
    App.scheduler.advance(1); assert((f.pulses==std::vector<uint32_t>{0,400}));
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE);
    std::cout << "PASS: rapid Stop waits for relay release and press separation\n";
  }
  if (scenario == 11) {
    reset_clock(); Fixture f; f.sensor(false); f.command("OPEN");
    App.scheduler.advance(1000); f.sensor(true); App.scheduler.advance(6000); f.command("STOP");
    assert(f.garage_door.position>0.49f && f.garage_door.position<0.51f);
    App.scheduler.advance(400); f.command("OPEN"); assert(f.pulses.size()==2);
    f.command("CLOSE"); App.scheduler.advance(5000); assert(f.pulses.size()==3);
    App.scheduler.advance(2000); f.command("STOP");
    assert(f.garage_door.position>0.32f && f.garage_door.position<0.34f);
    App.scheduler.advance(400); f.command("CLOSE"); assert(f.pulses.size()==4);
    f.command("OPEN"); assert(f.pulses.size()==5);
    App.scheduler.advance(9400);
    assert(f.garage_door.position==1.0f && f.garage_door.current_operation==COVER_OPERATION_IDLE);
    std::cout << "PASS: partial travel uses remaining time and rejects wrong-direction presses\n";
  }
  if (scenario == 12) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(17000);
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE && f.garage_door.position>0);
    f.sensor(false);
    assert(f.garage_door.position==0.0f && f.pulses.size()==1);
    std::cout << "PASS: elapsed closing time cannot claim a closed reed\n";
  }
  if (scenario == 13) {
    reset_clock(); Fixture f; f.open_ms=f.close_ms=0; f.sensor(false);
    f.command("OPEN"); f.command("TOGGLE"); f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(20000); assert(f.pulses.empty());
    std::cout << "PASS: uncalibrated controls cannot pulse the opener\n";
  }
  if (scenario == 14) {
    reset_clock(); Fixture f;
    f.command("OPEN"); f.command("CLOSE"); f.command("TOGGLE");
    App.scheduler.advance(20000); assert(f.pulses.empty());
    std::cout << "PASS: commands wait for the initial debounced sensor\n";
  }
  if (scenario == 15) {
    reset_clock(); Fixture f; f.sensor(true); f.command("CLOSE");
    App.scheduler.advance(5100); f.command("STOP"); f.sensor(false);
    App.scheduler.advance(20000);
    assert(f.pulses.size()==1 && f.garage_door.position==0.0f);
    std::cout << "PASS: closed reed cancels a Stop waiting for relay release\n";
  }
  if (scenario == 16) {
    reset_clock(); Fixture f; f.warning_trigger.trigger();
    App.scheduler.advance(2000); f.warning_trigger.trigger();
    App.scheduler.advance(3000); assert(f.warning_led && f.sound);
    App.scheduler.advance(2000); assert(!f.warning_led && !f.sound && f.pulses.empty());
    std::cout << "PASS: the shared manual warning button restarts its own timer\n";
  }
  if (scenario == 17) {
    reset_clock(); Fixture f; f.sensor(false); now=UINT32_MAX-1000; f.command("OPEN");
    App.scheduler.advance(1000); f.sensor(true); App.scheduler.advance(13000);
    assert(f.garage_door.position==1.0f && f.garage_door.current_operation==COVER_OPERATION_IDLE);
    std::cout << "PASS: travel timing survives millisecond clock wrap\n";
  }
  if (scenario == 18) {
    reset_clock(); Fixture f; f.sensor(false); f.command("OPEN");
    App.scheduler.advance(100); f.command("STOP"); App.scheduler.advance(350);
    // The reed cleared at 250ms but its 200ms debounce completes after Stop.
    f.sensor(true);
    assert(f.garage_door.position>0.02f && f.garage_door.position<0.04f);
    assert(f.garage_door.current_operation==COVER_OPERATION_IDLE && f.pulses.size()==2);
    std::cout << "PASS: delayed reed departure preserves a stopped partial position\n";
  }
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "dry-contact-sequences.cpp"
    exe = Path(directory) / "dry-contact-sequences"
    cpp.write_text(source)
    subprocess.run(["g++", "-std=c++20", "-O0", str(cpp), "-o", str(exe)], check=True)
    failures = []
    for scenario in range(1, 19):
        result = subprocess.run([str(exe), str(scenario)])
        if result.returncode:
            failures.append(scenario)
    assert not failures, f'Failed command scenarios: {failures}'

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

#include <string>
#include <utility>

#include "driver/gpio.h"
#include "esp_err.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
#include "esphome/components/wifi/wifi_component.h"
#endif

#include "binary_sensor/gdo_binary_sensor.h"
#include "cover/gdo_door.h"
#include "gdolib/gdo.h"
#include "light/gdo_light.h"
#include "lock/gdo_lock.h"
#include "number/gdo_number.h"
#include "select/gdo_select.h"
#include "sensor/gdo_sensor.h"
#include "switch/gdo_switch.h"
#include "text_sensor/gdo_text_sensor.h"

namespace esphome {
namespace secplus_gdo {

class GDOComponent : public Component
#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
    , public wifi::WiFiConnectStateListener
#endif
{
 public:
  void setup() override;
  void loop() override {}
  void dump_config() override;
  void on_shutdown() override { this->shutdown_driver_(); }

  // Keep BEFORE_CONNECTION so all entity preferences are loaded before startup.
  [[nodiscard]] float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  // Backward-compatible API used by older package YAML.
  void start_gdo();

  void set_obstruction_from_status(bool value) { this->gdo_conf_.obst_from_status = value; }
  void set_invert_uart(bool value) { this->gdo_conf_.invert_uart = value; }
  void set_uart_tx_pin(gpio_num_t pin) { this->gdo_conf_.uart_tx_pin = pin; }
  void set_uart_rx_pin(gpio_num_t pin) { this->gdo_conf_.uart_rx_pin = pin; }
  void set_obstruction_input_pin(gpio_num_t pin) { this->gdo_conf_.obst_in_pin = pin; }
  void set_min_command_interval(uint32_t ms) { this->min_command_interval_ms_ = ms; }
  void set_sync_retry_interval(uint32_t ms) { this->sync_retry_interval_ms_ = ms; }
  void set_auto_start(bool value) { this->auto_start_ = value; }

  esp_err_t request_sync();
  esp_err_t resync_opener();
  esp_err_t restart_comms();
  esp_err_t reset_travel_timings();

  esp_err_t door_open();
  esp_err_t door_close();
  esp_err_t door_stop();
  esp_err_t door_toggle();
  esp_err_t door_move_to(float position);

  esp_err_t light_on();
  esp_err_t light_off();
  esp_err_t light_toggle();

  esp_err_t lock();
  esp_err_t unlock();
  esp_err_t lock_toggle();

  void notify_cover_command() { this->cover_triggered_ = true; }

  void register_protocol_select(GDOSelect *select) { this->protocol_select_ = select; }
  void set_protocol_state(gdo_protocol_type_t protocol);

  void register_motion(std::function<void(bool)> &&f) { this->f_motion_.add(std::move(f)); }
  void register_motion_sensor(GDOBinarySensor *sensor) {
    this->register_motion([sensor](bool value) { sensor->publish_state(value); });
  }
  void set_motion_state(gdo_motion_state_t state) { this->f_motion_.call(state == GDO_MOTION_STATE_DETECTED); }

  void register_obstruction(std::function<void(bool)> &&f) { this->f_obstruction_.add(std::move(f)); }
  void register_obstruction_sensor(GDOBinarySensor *sensor) {
    this->register_obstruction([sensor](bool value) { sensor->publish_state(value); });
  }
  void set_obstruction(gdo_obstruction_state_t state) {
    this->f_obstruction_.call(state == GDO_OBSTRUCTION_STATE_OBSTRUCTED);
  }

  void register_button(std::function<void(bool)> &&f) { this->f_button_.add(std::move(f)); }
  void register_button_sensor(GDOBinarySensor *sensor) {
    this->register_button([sensor](bool value) { sensor->publish_state(value); });
  }
  void set_button_state(gdo_button_state_t state);

  void register_motor(std::function<void(bool)> &&f) { this->f_motor_.add(std::move(f)); }
  void register_motor_sensor(GDOBinarySensor *sensor) {
    this->register_motor([sensor](bool value) { sensor->publish_state(value); });
  }
  void set_motor_state(gdo_motor_state_t state);

  void register_wireless_remote(std::function<void(bool)> &&f) { this->f_wireless_remote_.add(std::move(f)); }
  void register_wireless_remote_sensor(GDOBinarySensor *sensor) {
    this->register_wireless_remote([sensor](bool value) { sensor->publish_state(value); });
  }

  void register_sync(std::function<void(bool)> &&f) { this->f_sync_.add(std::move(f)); }
  void register_sync_sensor(GDOBinarySensor *sensor) {
    this->register_sync([sensor](bool value) { sensor->publish_state(value); });
  }

  void register_battery(std::function<void(std::string)> &&f) { this->f_battery_.add(std::move(f)); }
  void register_battery_text_sensor(GDOTextSensor *sensor) {
    this->register_battery([sensor](const std::string &value) { sensor->publish_state(value); });
  }
  void set_battery_state(gdo_battery_state_t state);

  void register_openings(std::function<void(uint16_t)> &&f) { this->f_openings_.add(std::move(f)); }
  void register_openings_sensor(GDOStat *sensor) {
    this->register_openings([sensor](uint16_t value) { sensor->publish_state(value); });
  }
  void set_openings(uint16_t openings) { this->f_openings_.call(openings); }

  void register_paired_total(std::function<void(uint16_t)> &&f) { this->f_paired_total_.add(std::move(f)); }
  void register_paired_total_sensor(GDOStat *sensor) {
    this->register_paired_total([sensor](uint16_t value) { sensor->publish_state(value); });
  }

  void register_paired_remotes(std::function<void(uint16_t)> &&f) { this->f_paired_remotes_.add(std::move(f)); }
  void register_paired_remotes_sensor(GDOStat *sensor) {
    this->register_paired_remotes([sensor](uint16_t value) { sensor->publish_state(value); });
  }

  void register_paired_keypads(std::function<void(uint16_t)> &&f) { this->f_paired_keypads_.add(std::move(f)); }
  void register_paired_keypads_sensor(GDOStat *sensor) {
    this->register_paired_keypads([sensor](uint16_t value) { sensor->publish_state(value); });
  }

  void register_paired_wall_controls(std::function<void(uint16_t)> &&f) {
    this->f_paired_wall_controls_.add(std::move(f));
  }
  void register_paired_wall_controls_sensor(GDOStat *sensor) {
    this->register_paired_wall_controls([sensor](uint16_t value) { sensor->publish_state(value); });
  }

  void register_paired_accessories(std::function<void(uint16_t)> &&f) {
    this->f_paired_accessories_.add(std::move(f));
  }
  void register_paired_accessories_sensor(GDOStat *sensor) {
    this->register_paired_accessories([sensor](uint16_t value) { sensor->publish_state(value); });
  }

  void set_paired_device_counts(const gdo_paired_device_t &paired_devices);

  void register_protocol_text(std::function<void(std::string)> &&f) { this->f_protocol_text_.add(std::move(f)); }
  void register_protocol_text_sensor(GDOTextSensor *sensor) {
    this->register_protocol_text([sensor](const std::string &value) { sensor->publish_state(value); });
  }

  void register_gdolib_version_text(std::function<void(std::string)> &&f) {
    this->f_gdolib_version_text_.add(std::move(f));
  }
  void register_gdolib_version_text_sensor(GDOTextSensor *sensor) {
    this->register_gdolib_version_text([sensor](const std::string &value) { sensor->publish_state(value); });
  }

  void register_last_error_text(std::function<void(std::string)> &&f) {
    this->f_last_error_text_.add(std::move(f));
  }
  void register_last_error_text_sensor(GDOTextSensor *sensor) {
    this->register_last_error_text([sensor](const std::string &value) { sensor->publish_state(value); });
  }

  void register_last_event_text(std::function<void(std::string)> &&f) {
    this->f_last_event_text_.add(std::move(f));
  }
  void register_last_event_text_sensor(GDOTextSensor *sensor) {
    this->register_last_event_text([sensor](const std::string &value) { sensor->publish_state(value); });
  }

  void register_opener_id_text(std::function<void(std::string)> &&f) { this->f_opener_id_text_.add(std::move(f)); }
  void register_opener_id_text_sensor(GDOTextSensor *sensor) {
    this->register_opener_id_text([sensor](const std::string &value) { sensor->publish_state(value); });
  }

  void register_door(GDODoor *door) {
    this->door_ = door;
    door->set_parent(this);
  }
  void set_door_state(gdo_door_state_t state, float position) {
    if (this->door_) {
      this->door_->set_state(state, position);
    }
  }

  void register_light(GDOLight *light) {
    this->light_ = light;
    light->set_parent(this);
  }
  void set_light_state(gdo_light_state_t state) {
    if (this->light_) {
      this->light_->set_state(state);
    }
  }

  void register_lock(GDOLock *lock) {
    this->lock_ = lock;
    lock->set_parent(this);
  }
  void set_lock_state(gdo_lock_state_t state) {
    if (this->lock_) {
      this->lock_->set_state(state);
    }
  }

  void register_learn(GDOSwitch *sw) { this->learn_switch_ = sw; }
  void set_learn_state(gdo_learn_state_t state) {
    if (this->learn_switch_) {
      this->learn_switch_->write_state(state == GDO_LEARN_STATE_ACTIVE);
    }
  }

  void register_open_duration(GDONumber *num) { this->open_duration_ = num; }
  void set_open_duration(uint16_t ms) {
    if (this->open_duration_) {
      this->open_duration_->update_state(ms);
    }
  }

  void register_close_duration(GDONumber *num) { this->close_duration_ = num; }
  void set_close_duration(uint16_t ms) {
    if (this->close_duration_) {
      this->close_duration_->update_state(ms);
    }
  }

  void register_client_id(GDONumber *num) { this->client_id_ = num; }
  void set_client_id(uint32_t num) {
    if (this->client_id_) {
      this->client_id_->update_state(num);
    }
  }

  void register_rolling_code(GDONumber *num) { this->rolling_code_ = num; }
  void set_rolling_code(uint32_t num) {
    if (this->rolling_code_) {
      this->rolling_code_->update_state(num);
    }
  }

  void register_toggle_only(GDOSwitch *sw) { this->toggle_only_switch_ = sw; }
  void set_sync_state(bool synced);

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
  void on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) override;
#endif

 protected:
  static constexpr uint32_t START_RETRY_TIMEOUT_ID = 0x53475031;
  static constexpr uint32_t HEALTH_CHECK_INTERVAL_ID = 0x53475032;

  esp_err_t ensure_started_();
  esp_err_t apply_startup_preferences_();
  void shutdown_driver_();
  void schedule_start_retry_();
  void update_health_();
  void refresh_from_status_(const gdo_status_t &status);
  esp_err_t handle_command_result_(const char *command_name, esp_err_t err);
  void set_last_error_(const std::string &message);
  void clear_last_error_();
  void set_last_event_(const std::string &event_name);
  void set_opener_id_(uint32_t remote_id);
  static uint32_t millis_();

  gdo_status_t status_{};
  gdo_config_t gdo_conf_{
      .uart_num = UART_NUM_1,
      .obst_from_status = true,
      .invert_uart = true,
      .uart_tx_pin = GPIO_NUM_1,
      .uart_rx_pin = GPIO_NUM_2,
      .obst_in_pin = (gpio_num_t) -1,
  };

  CallbackManager<void(uint16_t)> f_openings_;
  CallbackManager<void(uint16_t)> f_paired_total_;
  CallbackManager<void(uint16_t)> f_paired_remotes_;
  CallbackManager<void(uint16_t)> f_paired_keypads_;
  CallbackManager<void(uint16_t)> f_paired_wall_controls_;
  CallbackManager<void(uint16_t)> f_paired_accessories_;
  CallbackManager<void(bool)> f_motion_;
  CallbackManager<void(bool)> f_obstruction_;
  CallbackManager<void(bool)> f_button_;
  CallbackManager<void(bool)> f_motor_;
  CallbackManager<void(bool)> f_wireless_remote_;
  CallbackManager<void(bool)> f_sync_;
  CallbackManager<void(std::string)> f_battery_;
  CallbackManager<void(std::string)> f_protocol_text_;
  CallbackManager<void(std::string)> f_gdolib_version_text_;
  CallbackManager<void(std::string)> f_last_error_text_;
  CallbackManager<void(std::string)> f_last_event_text_;
  CallbackManager<void(std::string)> f_opener_id_text_;

  GDODoor *door_{nullptr};
  GDOLight *light_{nullptr};
  GDOLock *lock_{nullptr};
  GDONumber *open_duration_{nullptr};
  GDONumber *close_duration_{nullptr};
  GDONumber *client_id_{nullptr};
  GDONumber *rolling_code_{nullptr};
  GDOSelect *protocol_select_{nullptr};
  GDOSwitch *learn_switch_{nullptr};
  GDOSwitch *toggle_only_switch_{nullptr};

  bool auto_start_{true};
  bool start_requested_{false};
  bool initialized_{false};
  bool started_{false};
  bool cover_triggered_{false};
  bool button_triggered_{false};
  bool sync_state_{false};
  bool force_resync_on_start_{false};
  uint8_t status_read_failures_{0};
  uint32_t min_command_interval_ms_{50};
  uint32_t sync_retry_interval_ms_{30000};
  uint32_t last_sync_attempt_ms_{0};
  uint32_t resync_client_id_{0x2908};
  std::string last_error_;
  std::string last_event_;
  std::string opener_id_;
};

template<typename... Ts> class SyncAction : public Action<Ts...> {
 public:
  explicit SyncAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->request_sync();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class ResyncAction : public Action<Ts...> {
 public:
  explicit ResyncAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->resync_opener();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class RestartCommsAction : public Action<Ts...> {
 public:
  explicit RestartCommsAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->restart_comms();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class ResetTravelTimingsAction : public Action<Ts...> {
 public:
  explicit ResetTravelTimingsAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->reset_travel_timings();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class DoorOpenAction : public Action<Ts...> {
 public:
  explicit DoorOpenAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->door_open();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class DoorCloseAction : public Action<Ts...> {
 public:
  explicit DoorCloseAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->door_close();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class DoorStopAction : public Action<Ts...> {
 public:
  explicit DoorStopAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->door_stop();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class DoorToggleAction : public Action<Ts...> {
 public:
  explicit DoorToggleAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->door_toggle();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class LightToggleAction : public Action<Ts...> {
 public:
  explicit LightToggleAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->light_toggle();
  }

 protected:
  GDOComponent *parent_;
};

template<typename... Ts> class LockToggleAction : public Action<Ts...> {
 public:
  explicit LockToggleAction(GDOComponent *parent) : parent_(parent) {}
  void play(Ts... x) override {
    ((void) x, ...);
    this->parent_->lock_toggle();
  }

 protected:
  GDOComponent *parent_;
};

}  // namespace secplus_gdo
}  // namespace esphome

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

#include "secplus_gdo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esphome/core/log.h"

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
#include "esphome/components/wifi/wifi_component.h"
#endif

static gpio_num_t g_panic_uart_tx_pin = GPIO_NUM_1;

namespace esphome {
namespace secplus_gdo {

static constexpr char TAG[] = "secplus_gdo";
static constexpr uint32_t WIRELESS_REMOTE_OFF_TIMEOUT_ID = 0x53475033;

static void gdo_event_handler(const gdo_status_t *status, gdo_cb_event_t event, void *arg) {
  auto *gdo = static_cast<GDOComponent *>(arg);
  if (gdo == nullptr || status == nullptr) {
    return;
  }

  switch (event) {
    case GDO_CB_EVENT_SYNCED:
      ESP_LOGI(TAG, "Synced: %s, protocol: %s", status->synced ? "true" : "false",
               gdo_protocol_type_to_string(status->protocol));
      if (status->protocol == GDO_PROTOCOL_SEC_PLUS_V2 && status->synced) {
        gdo->set_client_id(status->client_id);
        gdo->set_rolling_code(status->rolling_code);
      }
      if (!status->synced) {
        if (gdo_set_rolling_code(status->rolling_code + 100) == ESP_OK) {
          gdo_sync();
        }
      } else {
        gdo->set_protocol_state(status->protocol);
      }
      gdo->set_sync_state(status->synced);
      break;
    case GDO_CB_EVENT_LIGHT:
      gdo->set_light_state(status->light);
      break;
    case GDO_CB_EVENT_LOCK:
      gdo->set_lock_state(status->lock);
      break;
    case GDO_CB_EVENT_DOOR_POSITION: {
      int32_t clamped_position = status->door_position;
      if (clamped_position < 0) {
        clamped_position = 0;
      } else if (clamped_position > 10000) {
        clamped_position = 10000;
      }
      float position = static_cast<float>(10000 - clamped_position) / 10000.0f;
      gdo->set_door_state(status->door, position);
      if (status->door != GDO_DOOR_STATE_OPENING && status->door != GDO_DOOR_STATE_CLOSING) {
        gdo->set_motor_state(GDO_MOTOR_STATE_OFF);
      }
      break;
    }
    case GDO_CB_EVENT_LEARN:
      gdo->set_learn_state(status->learn);
      break;
    case GDO_CB_EVENT_OBSTRUCTION:
      gdo->set_obstruction(status->obstruction);
      break;
    case GDO_CB_EVENT_MOTION:
      gdo->set_motion_state(status->motion);
      break;
    case GDO_CB_EVENT_BATTERY:
      gdo->set_battery_state(status->battery);
      break;
    case GDO_CB_EVENT_BUTTON:
      gdo->set_button_state(status->button);
      break;
    case GDO_CB_EVENT_MOTOR:
      gdo->set_motor_state(status->motor);
      break;
    case GDO_CB_EVENT_OPENINGS:
      gdo->set_openings(status->openings);
      break;
    case GDO_CB_EVENT_TTC:
      ESP_LOGD(TAG, "Time to close: %u", status->ttc_seconds);
      break;
    case GDO_CB_EVENT_PAIRED_DEVICES:
      gdo->set_paired_device_counts(status->paired_devices);
      break;
    case GDO_CB_EVENT_OPEN_DURATION_MEASUREMENT:
      gdo->set_open_duration(status->open_ms);
      break;
    case GDO_CB_EVENT_CLOSE_DURATION_MEASUREMENT:
      gdo->set_close_duration(status->close_ms);
      break;
    default:
      ESP_LOGV(TAG, "Unhandled event: %d", event);
      break;
  }
}

void GDOComponent::setup() {
  g_panic_uart_tx_pin = this->gdo_conf_.uart_tx_pin;

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->add_connect_state_listener(this);
  }
#endif

  this->f_gdolib_version_text_.call(gdo_library_version());

  if (this->door_ != nullptr && this->toggle_only_switch_ != nullptr) {
    this->door_->set_toggle_only(this->toggle_only_switch_->state);
    this->toggle_only_switch_->set_control_function([this](bool value) { this->door_->set_toggle_only(value); });
  }

  if (this->auto_start_) {
    this->start_gdo();
  }

  this->set_interval(HEALTH_CHECK_INTERVAL_ID, 5000, [this]() { this->update_health_(); });
}

void GDOComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "secplus_gdo:");
  ESP_LOGCONFIG(TAG, "  TX pin: %d", this->gdo_conf_.uart_tx_pin);
  ESP_LOGCONFIG(TAG, "  RX pin: %d", this->gdo_conf_.uart_rx_pin);
  ESP_LOGCONFIG(TAG, "  Invert UART: %s", YESNO(this->gdo_conf_.invert_uart));
  ESP_LOGCONFIG(TAG, "  Obstruction from status: %s", YESNO(this->gdo_conf_.obst_from_status));
  if (!this->gdo_conf_.obst_from_status) {
    ESP_LOGCONFIG(TAG, "  Obstruction input pin: %d", this->gdo_conf_.obst_in_pin);
  }
  ESP_LOGCONFIG(TAG, "  Sync retry interval: %" PRIu32 " ms", this->sync_retry_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Min command interval: %" PRIu32 " ms", this->min_command_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Auto start: %s", YESNO(this->auto_start_));
  ESP_LOGCONFIG(TAG, "  gdolib version: %s", gdo_library_version());
}

void GDOComponent::start_gdo() {
  this->start_requested_ = true;
  this->ensure_started_();
}

esp_err_t GDOComponent::apply_startup_preferences_() {
  esp_err_t err = ESP_OK;

  if (this->protocol_select_ != nullptr && this->protocol_select_->has_selected_protocol()) {
    err = gdo_set_protocol(this->protocol_select_->get_selected_protocol());
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "Failed to apply protocol preference: %s", esp_err_to_name(err));
    }
  }

  if (this->toggle_only_switch_ != nullptr) {
    gdo_set_toggle_only(this->toggle_only_switch_->state);
    if (this->door_ != nullptr) {
      this->door_->set_toggle_only(this->toggle_only_switch_->state);
    }
  }

  if (this->open_duration_ != nullptr && this->open_duration_->has_state_value()) {
    uint32_t value = static_cast<uint32_t>(this->open_duration_->get_value());
    if (value <= UINT16_MAX) {
      err = gdo_set_open_duration(static_cast<uint16_t>(value));
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to apply opening duration preference: %s", esp_err_to_name(err));
      }
    }
  }

  if (this->close_duration_ != nullptr && this->close_duration_->has_state_value()) {
    uint32_t value = static_cast<uint32_t>(this->close_duration_->get_value());
    if (value <= UINT16_MAX) {
      err = gdo_set_close_duration(static_cast<uint16_t>(value));
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to apply closing duration preference: %s", esp_err_to_name(err));
      }
    }
  }

  if (this->force_resync_on_start_) {
    err = gdo_set_client_id(this->resync_client_id_);
    if (err == ESP_OK) {
      err = gdo_set_rolling_code(0);
      if (err == ESP_OK) {
        this->set_client_id(this->resync_client_id_);
        this->set_rolling_code(0);
      }
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to apply resync parameters: %s", esp_err_to_name(err));
    }
    this->force_resync_on_start_ = false;
  } else {
    if (this->client_id_ != nullptr && this->client_id_->has_state_value()) {
      err = gdo_set_client_id(static_cast<uint32_t>(this->client_id_->get_value()));
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to apply client ID preference: %s", esp_err_to_name(err));
      }
    }

    if (this->rolling_code_ != nullptr && this->rolling_code_->has_state_value()) {
      err = gdo_set_rolling_code(static_cast<uint32_t>(this->rolling_code_->get_value()));
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to apply rolling code preference: %s", esp_err_to_name(err));
      }
    }
  }

  return ESP_OK;
}

esp_err_t GDOComponent::ensure_started_() {
  if (!this->start_requested_) {
    return ESP_ERR_INVALID_STATE;
  }
  if (this->started_) {
    return ESP_OK;
  }

  esp_err_t err = ESP_OK;
  if (!this->initialized_) {
    err = gdo_init(&this->gdo_conf_);
    if (err != ESP_OK) {
      this->set_last_error_(std::string("gdo_init failed: ") + esp_err_to_name(err));
      this->schedule_start_retry_();
      return err;
    }
    this->initialized_ = true;
    gdo_set_min_command_interval(this->min_command_interval_ms_);
    this->apply_startup_preferences_();
  }

  err = gdo_start(gdo_event_handler, this);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    this->set_last_error_(std::string("gdo_start failed: ") + esp_err_to_name(err));
    this->schedule_start_retry_();
    return err;
  }

  this->started_ = true;
  this->status_read_failures_ = 0;
  this->clear_last_error_();
  this->set_last_event_("started");
  this->cancel_timeout(START_RETRY_TIMEOUT_ID);

  gdo_status_t status = {};
  if (gdo_get_status(&status) == ESP_OK) {
    this->refresh_from_status_(status);
  }

  return ESP_OK;
}

void GDOComponent::shutdown_driver_() {
  this->cancel_timeout(START_RETRY_TIMEOUT_ID);

  if (this->initialized_) {
    esp_err_t err = gdo_deinit();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "gdo_deinit failed: %s", esp_err_to_name(err));
    }
  }

  this->initialized_ = false;
  this->started_ = false;
  this->status_read_failures_ = 0;
  this->status_ = {};
  this->set_sync_state(false);
}

void GDOComponent::schedule_start_retry_() {
  this->set_timeout(START_RETRY_TIMEOUT_ID, 5000, [this]() {
    if (this->start_requested_ && !this->started_) {
      this->ensure_started_();
    }
  });
}

void GDOComponent::refresh_from_status_(const gdo_status_t &status) {
  gdo_status_t previous = this->status_;

  if (previous.protocol != status.protocol) {
    this->set_protocol_state(status.protocol);
  }

  if (previous.synced != status.synced) {
    this->set_sync_state(status.synced);
  }

  if (previous.openings != status.openings) {
    this->set_openings(status.openings);
  }

  if (memcmp(&previous.paired_devices, &status.paired_devices, sizeof(gdo_paired_device_t)) != 0) {
    this->set_paired_device_counts(status.paired_devices);
  }

  if (previous.open_ms != status.open_ms) {
    this->set_open_duration(status.open_ms);
  }

  if (previous.close_ms != status.close_ms) {
    this->set_close_duration(status.close_ms);
  }

  if (previous.client_id != status.client_id) {
    this->set_client_id(status.client_id);
  }

  if (previous.rolling_code != status.rolling_code) {
    this->set_rolling_code(status.rolling_code);
  }

  if (previous.remote_id != status.remote_id) {
    this->set_opener_id_(status.remote_id);
  }

  if (previous.light != status.light) {
    this->set_light_state(status.light);
  }

  if (previous.lock != status.lock) {
    this->set_lock_state(status.lock);
  }

  if (previous.learn != status.learn) {
    this->set_learn_state(status.learn);
  }

  if (previous.obstruction != status.obstruction) {
    this->set_obstruction(status.obstruction);
  }

  if (previous.motion != status.motion) {
    this->set_motion_state(status.motion);
  }

  if (previous.button != status.button) {
    this->set_button_state(status.button);
  }

  if (previous.motor != status.motor) {
    this->set_motor_state(status.motor);
  }

  if (previous.battery != status.battery) {
    this->set_battery_state(status.battery);
  }

  if (previous.door != status.door || previous.door_position != status.door_position) {
    int32_t clamped_position = status.door_position;
    if (clamped_position < 0) {
      clamped_position = 0;
    } else if (clamped_position > 10000) {
      clamped_position = 10000;
    }
    float position = static_cast<float>(10000 - clamped_position) / 10000.0f;
    this->set_door_state(status.door, position);
  }

  this->status_ = status;
}

void GDOComponent::update_health_() {
  if (!this->start_requested_) {
    return;
  }
  if (!this->started_) {
    this->ensure_started_();
    return;
  }

  gdo_status_t status = {};
  if (gdo_get_status(&status) != ESP_OK) {
    this->status_read_failures_++;
    this->set_last_error_("failed to read status");

    if (this->status_read_failures_ >= 3) {
      ESP_LOGW(TAG, "GDO status health check failed 3 times; restarting communications");
      this->status_read_failures_ = 0;
      this->restart_comms();
    }
    return;
  }

  this->status_read_failures_ = 0;
  this->refresh_from_status_(status);

  if (!status.synced) {
    uint32_t now = millis_();
    if (now - this->last_sync_attempt_ms_ >= this->sync_retry_interval_ms_) {
      this->last_sync_attempt_ms_ = now;
      this->request_sync();
    }
  }
}

esp_err_t GDOComponent::handle_command_result_(const char *command_name, esp_err_t err) {
  if (err == ESP_OK) {
    this->set_last_event_(command_name);
    this->clear_last_error_();
    return ESP_OK;
  }

  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%s failed: %s", command_name, esp_err_to_name(err));
  this->set_last_error_(buffer);
  ESP_LOGW(TAG, "%s", buffer);
  return err;
}

void GDOComponent::set_last_error_(const std::string &message) {
  if (message == this->last_error_) {
    return;
  }
  this->last_error_ = message;
  this->f_last_error_text_.call(this->last_error_);
  this->status_set_warning();
}

void GDOComponent::clear_last_error_() {
  if (this->last_error_.empty()) {
    return;
  }
  this->last_error_.clear();
  this->f_last_error_text_.call(this->last_error_);
  this->status_clear_warning();
}

void GDOComponent::set_last_event_(const std::string &event_name) {
  if (event_name == this->last_event_) {
    return;
  }
  this->last_event_ = event_name;
  this->f_last_event_text_.call(this->last_event_);
}

void GDOComponent::set_opener_id_(uint32_t remote_id) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "0x%08" PRIX32, remote_id);
  std::string opener_id(buffer);
  if (opener_id == this->opener_id_) {
    return;
  }
  this->opener_id_ = opener_id;
  this->f_opener_id_text_.call(this->opener_id_);
}

uint32_t GDOComponent::millis_() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }

esp_err_t GDOComponent::request_sync() {
  if (!this->start_requested_) {
    this->start_requested_ = true;
  }
  esp_err_t err = this->ensure_started_();
  if (err != ESP_OK) {
    return err;
  }

  err = gdo_sync();
  if (err == ESP_OK || err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_FINISHED) {
    this->set_last_event_("sync");
    return ESP_OK;
  }
  return this->handle_command_result_("sync", err);
}

esp_err_t GDOComponent::resync_opener() {
  uint32_t random_upper = (esp_random() & 0x7F7FU) << 16U;
  this->resync_client_id_ = random_upper | 0x2908U;
  this->force_resync_on_start_ = true;
  this->set_last_event_("resync");

  if (this->client_id_ != nullptr) {
    this->client_id_->update_state(this->resync_client_id_);
  }
  if (this->rolling_code_ != nullptr) {
    this->rolling_code_->update_state(0);
  }

  return this->restart_comms();
}

esp_err_t GDOComponent::restart_comms() {
  this->set_last_event_("restart");
  this->start_requested_ = true;
  this->shutdown_driver_();
  return this->ensure_started_();
}

esp_err_t GDOComponent::reset_travel_timings() {
  esp_err_t err_open = gdo_set_open_duration(0);
  esp_err_t err_close = gdo_set_close_duration(0);
  if (this->open_duration_ != nullptr) {
    this->open_duration_->update_state(0);
  }
  if (this->close_duration_ != nullptr) {
    this->close_duration_->update_state(0);
  }

  if (err_open != ESP_OK) {
    return this->handle_command_result_("reset_travel_timings", err_open);
  }
  if (err_close != ESP_OK) {
    return this->handle_command_result_("reset_travel_timings", err_close);
  }
  return this->handle_command_result_("reset_travel_timings", ESP_OK);
}

esp_err_t GDOComponent::door_open() {
  this->notify_cover_command();
  return this->handle_command_result_("door_open", gdo_door_open());
}

esp_err_t GDOComponent::door_close() {
  this->notify_cover_command();
  return this->handle_command_result_("door_close", gdo_door_close());
}

esp_err_t GDOComponent::door_stop() {
  this->notify_cover_command();
  return this->handle_command_result_("door_stop", gdo_door_stop());
}

esp_err_t GDOComponent::door_toggle() {
  this->notify_cover_command();
  return this->handle_command_result_("door_toggle", gdo_door_toggle());
}

esp_err_t GDOComponent::door_move_to(float position) {
  float clamped = std::max(0.0f, std::min(1.0f, position));
  uint32_t target = static_cast<uint32_t>(std::lround(10000.0f - (clamped * 10000.0f)));
  this->notify_cover_command();
  return this->handle_command_result_("door_move_to", gdo_door_move_to_target(target));
}

esp_err_t GDOComponent::light_on() { return this->handle_command_result_("light_on", gdo_light_on()); }

esp_err_t GDOComponent::light_off() { return this->handle_command_result_("light_off", gdo_light_off()); }

esp_err_t GDOComponent::light_toggle() { return this->handle_command_result_("light_toggle", gdo_light_toggle()); }

esp_err_t GDOComponent::lock() { return this->handle_command_result_("lock", gdo_lock()); }

esp_err_t GDOComponent::unlock() { return this->handle_command_result_("unlock", gdo_unlock()); }

esp_err_t GDOComponent::lock_toggle() { return this->handle_command_result_("lock_toggle", gdo_toggle_lock()); }

void GDOComponent::set_protocol_state(gdo_protocol_type_t protocol) {
  if (this->protocol_select_ != nullptr) {
    this->protocol_select_->update_state(protocol);
  }
  this->f_protocol_text_.call(gdo_protocol_type_to_string(protocol));
}

void GDOComponent::set_button_state(gdo_button_state_t state) {
  if (state == GDO_BUTTON_STATE_PRESSED) {
    this->button_triggered_ = true;
    if (this->door_ != nullptr) {
      this->door_->cancel_pre_close_warning();
    }
  }
  this->f_button_.call(state == GDO_BUTTON_STATE_PRESSED);
}

void GDOComponent::set_motor_state(gdo_motor_state_t state) {
  this->f_motor_.call(state == GDO_MOTOR_STATE_ON);
  if (state == GDO_MOTOR_STATE_ON) {
    if (!this->button_triggered_ && !this->cover_triggered_) {
      this->f_wireless_remote_.call(true);
      this->set_timeout(WIRELESS_REMOTE_OFF_TIMEOUT_ID, 500, [this]() { this->f_wireless_remote_.call(false); });
    }
    this->button_triggered_ = false;
    this->cover_triggered_ = false;
  }
}

void GDOComponent::set_battery_state(gdo_battery_state_t state) {
  if (state == GDO_BATT_STATE_UNKNOWN) {
    return;
  }
  this->f_battery_.call(gdo_battery_state_to_string(state));
}

void GDOComponent::set_paired_device_counts(const gdo_paired_device_t &paired_devices) {
  this->f_paired_total_.call(paired_devices.total_all);
  this->f_paired_remotes_.call(paired_devices.total_remotes);
  this->f_paired_keypads_.call(paired_devices.total_keypads);
  this->f_paired_wall_controls_.call(paired_devices.total_wall_controls);
  this->f_paired_accessories_.call(paired_devices.total_accessories);
}

void GDOComponent::set_sync_state(bool synced) {
  this->sync_state_ = synced;
  if (this->door_ != nullptr) {
    this->door_->set_sync_state(synced);
  }
  if (this->light_ != nullptr) {
    this->light_->set_sync_state(synced);
  }
  if (this->lock_ != nullptr) {
    this->lock_->set_sync_state(synced);
  }
  this->f_sync_.call(synced);
  if (synced) {
    this->status_clear_warning();
  }
}

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
void GDOComponent::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  (void) bssid;
  if (ssid.empty()) {
    this->set_last_event_("wifi_disconnected");
    return;
  }

  this->set_last_event_("wifi_connected");
  if (!this->start_requested_) {
    return;
  }

  if (!this->started_) {
    this->ensure_started_();
    return;
  }

  this->request_sync();
}
#endif

}  // namespace secplus_gdo
}  // namespace esphome

// Need to wrap the panic handler to disable the GDO TX pin and pull the output high to
// prevent spuriously triggering the GDO to open when the ESP32 panics.
extern "C" {
#include "hal/gpio_hal.h"

void __real_esp_panic_handler(void *);

void __wrap_esp_panic_handler(void *info) {
  esp_rom_printf("PANIC: DISABLING GDO UART TX PIN!\n");
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[g_panic_uart_tx_pin], PIN_FUNC_GPIO);
  gpio_set_direction(g_panic_uart_tx_pin, GPIO_MODE_INPUT);
  gpio_pulldown_en(g_panic_uart_tx_pin);

  // Call the original panic handler
  __real_esp_panic_handler(info);
}
}  // extern "C"

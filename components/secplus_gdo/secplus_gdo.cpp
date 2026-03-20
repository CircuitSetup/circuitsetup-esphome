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

#include "driver/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "inttypes.h"

namespace esphome {
namespace secplus_gdo {

    constexpr char TAG[] = "secplus_gdo";

    static void gdo_event_handler(const gdo_status_t *status, gdo_cb_event_t event, void *arg) {
        auto *gdo = static_cast<GDOComponent *>(arg);
        if (gdo == nullptr || status == nullptr) {
            ESP_LOGE(TAG, "Received invalid callback state from gdolib");
            return;
        }

        switch (event) {
        case GDO_CB_EVENT_SYNCED:
            ESP_LOGI(TAG, "Synced: %s, protocol: %s", status->synced ? "true" : "false",
                     gdo_protocol_type_to_string(status->protocol));
            if (status->protocol == GDO_PROTOCOL_SEC_PLUS_V2) {
                ESP_LOGI(TAG, "Client ID: %" PRIu32 ", Rolling code: %" PRIu32, status->client_id, status->rolling_code);
                if (status->synced) {
                    // Save the last successful ClientID rolling code value to NVS for use on reboot
                    gdo->set_client_id(status->client_id);
                    gdo->set_rolling_code(status->rolling_code);
                }
            }

            if (!status->synced) {
                const auto next_rolling_code = status->rolling_code + 100;
                if (gdo_set_rolling_code(next_rolling_code) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set rolling code");
                } else {
                    ESP_LOGI(TAG, "Rolling code set to %" PRIu32 ", retrying sync", next_rolling_code);
                    const auto err = gdo_sync();
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to start resync: %s", esp_err_to_name(err));
                    }
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
            const float position = static_cast<float>(10000 - status->door_position) / 10000.0f;
            gdo->set_door_state(status->door, position);
            if (status->door != GDO_DOOR_STATE_OPENING && status->door != GDO_DOOR_STATE_CLOSING) {
                gdo->set_motor_state(GDO_MOTOR_STATE_OFF);
            }
            break;
        }
        case GDO_CB_EVENT_LEARN:
            ESP_LOGI(TAG, "Learn: %s", gdo_learn_state_to_string(status->learn));
            gdo->set_learn_state(status->learn);
            break;
        case GDO_CB_EVENT_OBSTRUCTION:
            ESP_LOGI(TAG, "Obstruction: %s", gdo_obstruction_state_to_string(status->obstruction));
            gdo->set_obstruction(status->obstruction);
            break;
        case GDO_CB_EVENT_MOTION:
            ESP_LOGI(TAG, "Motion: %s", gdo_motion_state_to_string(status->motion));
            gdo->set_motion_state(status->motion);
            break;
        case GDO_CB_EVENT_BATTERY:
            ESP_LOGI(TAG, "Battery: %s", gdo_battery_state_to_string(status->battery));
            gdo->set_battery_state(status->battery);
            break;
        case GDO_CB_EVENT_BUTTON:
            ESP_LOGI(TAG, "Button: %s", gdo_button_state_to_string(status->button));
            gdo->set_button_state(status->button);
            break;
        case GDO_CB_EVENT_MOTOR:
            ESP_LOGI(TAG, "Motor: %s", gdo_motor_state_to_string(status->motor));
            gdo->set_motor_state(status->motor);
            break;
        case GDO_CB_EVENT_OPENINGS:
            ESP_LOGI(TAG, "Openings: %d", status->openings);
            gdo->set_openings(status->openings);
            break;
        case GDO_CB_EVENT_TTC:
            ESP_LOGI(TAG, "Time to close: %d", status->ttc_seconds);
            break;
        case GDO_CB_EVENT_PAIRED_DEVICES:
            ESP_LOGI(TAG, "Paired devices: %d remotes, %d keypads, %d wall controls, %d accessories, %d total",
                     status->paired_devices.total_remotes, status->paired_devices.total_keypads,
                     status->paired_devices.total_wall_controls, status->paired_devices.total_accessories,
                     status->paired_devices.total_all);
            gdo->set_paired_devices(status->paired_devices);
            break;
        case GDO_CB_EVENT_OPEN_DURATION_MEASUREMENT:
            ESP_LOGI(TAG, "Open duration: %d", status->open_ms);
            gdo->set_open_duration(status->open_ms);
            break;
        case GDO_CB_EVENT_CLOSE_DURATION_MEASUREMENT:
            ESP_LOGI(TAG, "Close duration: %d", status->close_ms);
            gdo->set_close_duration(status->close_ms);
            break;
        default:
            ESP_LOGI(TAG, "Unknown event: %d", event);
            break;
        }
    }

    void GDOComponent::start_gdo() {
        this->start_requested_ = true;
        this->start_if_ready_();
    }

    void GDOComponent::register_binary_sensor(GDOBinarySensor *sensor) {
        if (sensor == nullptr) {
            return;
        }

        switch (sensor->get_type()) {
        case GDOBinarySensorType::MOTION:
            this->motion_sensor_ = sensor;
            break;
        case GDOBinarySensorType::OBSTRUCTION:
            this->obstruction_sensor_ = sensor;
            break;
        case GDOBinarySensorType::MOTOR:
            this->motor_sensor_ = sensor;
            break;
        case GDOBinarySensorType::BUTTON:
            this->button_sensor_ = sensor;
            break;
        case GDOBinarySensorType::SYNC:
            this->sync_sensor_ = sensor;
            break;
        case GDOBinarySensorType::WIRELESS_REMOTE:
            this->wireless_remote_sensor_ = sensor;
            break;
        }
    }

    void GDOComponent::register_sensor(GDOStat *sensor) {
        if (sensor == nullptr) {
            return;
        }

        switch (sensor->get_type()) {
        case GDOStatType::OPENINGS:
            this->openings_sensor_ = sensor;
            break;
        case GDOStatType::PAIRED_DEVICES_TOTAL:
            this->paired_total_sensor_ = sensor;
            break;
        case GDOStatType::PAIRED_DEVICES_REMOTES:
            this->paired_remotes_sensor_ = sensor;
            break;
        case GDOStatType::PAIRED_DEVICES_KEYPADS:
            this->paired_keypads_sensor_ = sensor;
            break;
        case GDOStatType::PAIRED_DEVICES_WALL_CONTROLS:
            this->paired_wall_controls_sensor_ = sensor;
            break;
        case GDOStatType::PAIRED_DEVICES_ACCESSORIES:
            this->paired_accessories_sensor_ = sensor;
            break;
        }
    }

    void GDOComponent::register_text_sensor(GDOTextSensor *sensor) {
        if (sensor == nullptr) {
            return;
        }

        switch (sensor->get_type()) {
        case GDOTextSensorType::BATTERY:
            this->battery_sensor_ = sensor;
            break;
        }
    }

    void GDOComponent::register_number(GDONumber *num) {
        if (num == nullptr) {
            return;
        }

        switch (num->get_type()) {
        case GDONumberType::OPEN_DURATION:
            this->open_duration_ = num;
            num->set_control_function([](float value) { return gdo_set_open_duration(static_cast<uint16_t>(value)); });
            break;
        case GDONumberType::CLOSE_DURATION:
            this->close_duration_ = num;
            num->set_control_function([](float value) { return gdo_set_close_duration(static_cast<uint16_t>(value)); });
            break;
        case GDONumberType::CLIENT_ID:
            this->client_id_ = num;
            num->set_control_function([](float value) { return gdo_set_client_id(static_cast<uint32_t>(value)); });
            break;
        case GDONumberType::ROLLING_CODE:
            this->rolling_code_ = num;
            num->set_control_function([](float value) { return gdo_set_rolling_code(static_cast<uint32_t>(value)); });
            break;
        }
    }

    void GDOComponent::register_switch(GDOSwitch *sw) {
        if (sw == nullptr) {
            return;
        }

        switch (sw->get_type()) {
        case SwitchType::LEARN:
            this->learn_switch_ = sw;
            break;
        case SwitchType::TOGGLE_ONLY:
            this->toggle_only_switch_ = sw;
            break;
        }
    }

    void GDOComponent::set_motion_state(gdo_motion_state_t state) {
        if (this->motion_sensor_ != nullptr) {
            this->motion_sensor_->publish(state == GDO_MOTION_STATE_DETECTED);
        }
    }

    void GDOComponent::set_obstruction(gdo_obstruction_state_t state) {
        if (this->obstruction_sensor_ != nullptr) {
            this->obstruction_sensor_->publish(state == GDO_OBSTRUCTION_STATE_OBSTRUCTED);
        }
    }

    void GDOComponent::set_button_state(gdo_button_state_t state) {
        if (state == GDO_BUTTON_STATE_PRESSED) {
            this->button_triggered_ = true;
            if (this->door_ != nullptr) {
                this->door_->cancel_pre_close_warning();
            }
        }

        if (this->button_sensor_ != nullptr) {
            this->button_sensor_->publish(state == GDO_BUTTON_STATE_PRESSED);
        }
    }

    void GDOComponent::set_motor_state(gdo_motor_state_t state) {
        if (this->motor_sensor_ != nullptr) {
            this->motor_sensor_->publish(state == GDO_MOTOR_STATE_ON);
        }

        if (state == GDO_MOTOR_STATE_ON) {
            if (!this->button_triggered_ && !this->cover_triggered_ && this->wireless_remote_sensor_ != nullptr) {
                this->wireless_remote_sensor_->publish(true);
                this->set_timeout("wireless_remote_off", 500, [this]() {
                    if (this->wireless_remote_sensor_ != nullptr) {
                        this->wireless_remote_sensor_->publish(false);
                    }
                });
            }
            this->button_triggered_ = false;
            this->cover_triggered_ = false;
        }
    }

    void GDOComponent::set_battery_state(gdo_battery_state_t state) {
        if (this->battery_sensor_ != nullptr && state != GDO_BATT_STATE_UNKNOWN) {
            this->battery_sensor_->update_state(gdo_battery_state_to_string(state));
        }
    }

    void GDOComponent::set_openings(uint16_t openings) {
        if (this->openings_sensor_ != nullptr) {
            this->openings_sensor_->update_state(openings);
        }
    }

    void GDOComponent::set_paired_devices(const gdo_paired_device_t &paired_devices) {
        if (this->paired_total_sensor_ != nullptr) {
            this->paired_total_sensor_->update_state(paired_devices.total_all);
        }
        if (this->paired_remotes_sensor_ != nullptr) {
            this->paired_remotes_sensor_->update_state(paired_devices.total_remotes);
        }
        if (this->paired_keypads_sensor_ != nullptr) {
            this->paired_keypads_sensor_->update_state(paired_devices.total_keypads);
        }
        if (this->paired_wall_controls_sensor_ != nullptr) {
            this->paired_wall_controls_sensor_->update_state(paired_devices.total_wall_controls);
        }
        if (this->paired_accessories_sensor_ != nullptr) {
            this->paired_accessories_sensor_->update_state(paired_devices.total_accessories);
        }
    }

    void GDOComponent::sync_toggle_only_() {
        bool toggle_only = this->status_.toggle_only;
        if (this->toggle_only_switch_ != nullptr) {
            this->toggle_only_switch_->set_control_function([this](bool state) {
                this->status_.toggle_only = state;
                if (this->door_ != nullptr) {
                    this->door_->set_toggle_only(state);
                }
                if (this->initialized_) {
                    gdo_set_toggle_only(state);
                }
            });
            toggle_only = this->toggle_only_switch_->state;
        }

        this->status_.toggle_only = toggle_only;
        if (this->door_ != nullptr) {
            this->door_->set_toggle_only(toggle_only);
        }
        if (this->initialized_) {
            gdo_set_toggle_only(toggle_only);
        }
    }

    void GDOComponent::start_if_ready_() {
        if (!this->initialized_ || this->started_ || !this->start_requested_) {
            return;
        }

        const auto err = gdo_start(gdo_event_handler, this);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start secplus GDO: %s", esp_err_to_name(err));
            return;
        }

        this->started_ = true;
        ESP_LOGI(TAG, "secplus GDO started");
    }

    void GDOComponent::setup() {
        this->status_ = {};

        // Initialize the driver first so child entities can restore saved preferences before we start it.
        gdo_config_t gdo_conf = {
            .uart_num = UART_NUM_1,
            .obst_from_status = true,
            .invert_uart = true,
            .uart_tx_pin = (gpio_num_t) GDO_UART_TX_PIN,
            .uart_rx_pin = (gpio_num_t) GDO_UART_RX_PIN,
            .obst_in_pin = (gpio_num_t) -1,
        };

        const auto init_err = gdo_init(&gdo_conf);
        if (init_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize secplus GDO: %s", esp_err_to_name(init_err));
            this->mark_failed();
            return;
        }

        this->initialized_ = true;

        const auto status_err = gdo_get_status(&this->status_);
        if (status_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load initial GDO status: %s", esp_err_to_name(status_err));
        }

        this->sync_toggle_only_();
        this->defer([this]() { this->start_if_ready_(); });
    }

    void GDOComponent::dump_config() {
        ESP_LOGCONFIG(TAG, "secplus GDO:");
        ESP_LOGCONFIG(TAG, "  UART TX pin: %d", GDO_UART_TX_PIN);
        ESP_LOGCONFIG(TAG, "  UART RX pin: %d", GDO_UART_RX_PIN);
        ESP_LOGCONFIG(TAG, "  Initialized: %s", YESNO(this->initialized_));
        ESP_LOGCONFIG(TAG, "  Start requested: %s", YESNO(this->start_requested_));
        ESP_LOGCONFIG(TAG, "  Started: %s", YESNO(this->started_));
        ESP_LOGCONFIG(TAG, "  Cover registered: %s", YESNO(this->door_ != nullptr));
        ESP_LOGCONFIG(TAG, "  Light registered: %s", YESNO(this->light_ != nullptr));
        ESP_LOGCONFIG(TAG, "  Lock registered: %s", YESNO(this->lock_ != nullptr));
        ESP_LOGCONFIG(TAG, "  Protocol select registered: %s", YESNO(this->protocol_select_ != nullptr));
        ESP_LOGCONFIG(TAG, "  Toggle-only switch registered: %s", YESNO(this->toggle_only_switch_ != nullptr));
        ESP_LOGCONFIG(TAG, "  Learn switch registered: %s", YESNO(this->learn_switch_ != nullptr));
    }

    void GDOComponent::on_shutdown() {
        if (!this->initialized_) {
            return;
        }

        const auto err = gdo_deinit();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinitialize secplus GDO: %s", esp_err_to_name(err));
            return;
        }

        this->initialized_ = false;
        this->started_ = false;
    }

    void GDOComponent::set_sync_state(bool synced) {
        this->status_.synced = synced;

        if (this->door_ != nullptr) {
            this->door_->set_sync_state(synced);
        }

        if (this->light_ != nullptr) {
            this->light_->set_sync_state(synced);
        }

        if (this->lock_ != nullptr) {
            this->lock_->set_sync_state(synced);
        }

        if (this->sync_sensor_ != nullptr) {
            this->sync_sensor_->publish(synced);
        }
    }

} // namespace secplus_gdo
} // namespace esphome

// Need to wrap the panic handler to disable the GDO TX pin and pull the output high to
// prevent spuriously triggering the GDO to open when the ESP32 panics.
// ESPHome 2026.3.0+ provides its own ESP32 crash handler wrapper, so only install the
// local wrapper when that built-in handler is not in use.
#if !defined(USE_ESP32_CRASH_HANDLER)
extern "C" {
#include "hal/gpio_hal.h"

void __real_esp_panic_handler(void *);

void __wrap_esp_panic_handler(void *info) {
    esp_rom_printf("PANIC: DISABLING GDO UART TX PIN!\n");
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[(gpio_num_t) GDO_UART_TX_PIN], PIN_FUNC_GPIO);
    gpio_set_direction((gpio_num_t) GDO_UART_TX_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_en((gpio_num_t) GDO_UART_TX_PIN);

    // Call the original panic handler
    __real_esp_panic_handler(info);
}
} // extern "C"
#endif

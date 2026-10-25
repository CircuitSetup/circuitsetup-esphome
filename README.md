# CircuitSetup ESPHome

ESPHome configurations and a custom `secplus_gdo` external component for CircuitSetup garage door opener hardware.
This repository builds on the original garage door opener work from [Konnected Inc.](https://github.com/konnected-io/konnected-esphome), whose earlier ESPHome support helped establish the foundation for this project.

## Choose a Starting Point

Use CircuitSetup garage door firmware in one of these four ways:

1. [CircuitSetup ESP Web Installer](https://circuitsetup.github.io/ESPWebInstaller/)
   The quickest path if you want pre-compiled firmware and browser-based flashing.
2. [`circuitsetup-gdo-default.yaml`](circuitsetup-gdo-default.yaml)
   The simplest starter/import path for CircuitSetup Security+ hardware.
3. [`circuitsetup-secplus-garage-door-opener.yaml`](circuitsetup-secplus-garage-door-opener.yaml)
   The full editable reference config for the Security+ opener.
4. [`components/secplus_gdo/README.md`](components/secplus_gdo/README.md)
   The advanced path for direct `external_components` usage and fully custom YAML.

For most users, start with the default or full Security+ YAML and only edit the documented substitutions near the top of the file.

## Quick Start

For CircuitSetup Security+ hardware, the quickest starting point is:

```yaml
packages:
  circuitsetup.secplus-garage-door-opener: github://CircuitSetup/circuitsetup-esphome/circuitsetup-gdo-default.yaml@master
```

That starter file pulls in the full Security+ package bundle from `master`, including the pinned `gdolib` dependency.

## Common Customizations

The full Security+ YAML now has a supported "safe customization surface" near the top of the file. Most users should only need to edit:

- Device name and friendly name
- `garage_label`, which renames the common Garage Door / Garage Light / Garage Openings / Garage Temp / Garage Humidity entities together
- Close-warning duration
- Temperature update interval and sensor offsets
- Logger level
- Wi-Fi response tuning

Example:

```yaml
substitutions:
  name: shop-gdo
  friendly_name: Shop Door Opener
  garage_label: Shop
  garage_door_close_warning_duration: 8s
  temp_update_interval: 30s
  logger_level: INFO
  wifi_power_save_mode: NONE
  wifi_post_connect_roaming: "false"
```

## Security Settings

For Home Assistant 2026.2+ deployments, the shipped YAML documents the recommended secrets-based security settings:

```yaml
api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password
```

## When You Need Local YAML Edits

Use the full Security+ YAML for common name, timing, logger, and Wi-Fi changes.

Switch to a local copy of the full YAML when you want to:

- Remove or replace packages such as `temp-sensor.yaml`
- Change package internals instead of top-level substitutions
- Edit the custom `secplus_gdo` component locally

Remote package composition works well for normal setup, but it is not the best fit for deep feature toggles. For those cases, use a local checkout and the local `external_components` / local package workflow documented in [`components/secplus_gdo/README.md`](components/secplus_gdo/README.md).

## Advanced Usage

If you want to use the external component directly instead of the full package bundle, see [`components/secplus_gdo/README.md`](components/secplus_gdo/README.md).

The component now starts itself during setup, so custom configs no longer need a hidden `wifi.on_connect` hook to call `id(cs_gdo).start_gdo()`.

## Dry-contact setup and calibration

Dry-contact firmware `1.2` uses one closed-position contact on Aux In 1/GPIO5 (or
Aux In 2/GPIO6 by overriding `wired_sensor_pin`) and a momentary opener output on
Aux Out/GPIO7. The unused auxiliary input remains available for another sensor.
The contact must read OFF when fully closed and ON when away from closed.

The device exposes **Calibrate Travel Times**, **Travel Time Calibrated**, and
**Travel Time Calibration** in Home Assistant and its ESPHome web page. The last
two show whether setup is complete and the current step. **Calibrated Opening
Time** and **Calibrated Closing Time** show the saved measurements in seconds.
Normal Open, Close, and Toggle controls cannot move an uncalibrated door.

Calibrate while watching the door and keeping the doorway clear:

1. Use the wall control to put the door fully closed and let it stop. Press
   **Calibrate Travel Times**. This deliberate calibration action starts opening
   even though ordinary movement controls are locked.
2. Press the same button again **immediately when the door reaches fully open**.
   The single closed contact cannot detect this endpoint, so this confirmation
   records the opening time. The device then gives its normal closing warning.
3. The device starts closing and measures until the closed contact confirms
   arrival. **Travel Time Calibrated** turns ON and the status reads **Calibrated**.
   Both times are saved together and survive a restart; no YAML time edits are
   required.

**Stop** cancels calibration and remains available during calibration movement.
Ordinary Open, Close, and Toggle requests are ignored throughout the calibration
cycle. Recalibrating clears the old saved times before its first movement. An
interrupted cycle, failed save, or missing endpoint confirmation keeps normal
controls locked, including after a restart. To retry, use the wall control to
return fully closed, then press the calibration button again.

Each measured direction must take at least 2 seconds and less than 120 seconds.
After a two-minute timeout the device cancels pending work and reports failure;
it does not send a blind pulse that might restart a door already at its limit.
Use the wall control if physical movement needs to be stopped after a timeout.

The firmware estimates movement and partial position from the learned times; it
reports fully closed only when the contact confirms it. The opener must stop
itself at its normal travel limits. No automatic relay pulse is sent when a
normal travel timer expires. Pressing the full-open confirmation late inflates
the opening estimate, so watch the door closely during calibration.

This configuration assumes a wall-button sequence of **open → stop → close →
stop → open**. A directional request that would send the door the wrong way is
ignored and logged. Stop followed by Toggle reverses the next movement; any
movement toward closed gets the warning first. Openers that reverse immediately
on a press while closing need a different control sequence and are not supported
by this configuration.

Stop remains effective after an opening door leaves the closed contact. Repeated
Open/Close requests do not add another relay pulse or restart an active countdown.
Stop or Toggle cancels a pending close, and Toggle during movement stops it without
waiting through another warning. Open also cancels a pending close. An old warning
timer cannot silence a newer warning. If the contact closes during the countdown,
the controller cancels the close without pulsing the opener.

A Stop arriving while the relay is still held waits for the current 300 ms press
(or your configured momentary duration) and a 100 ms release gap. New movement
commands during this short interval are ignored; send them after the press ends.

One contact cannot detect an external wall-button stop, an obstruction reversal,
or full opening. Start or reboot with the door stationary at a travel limit; an
inactive closed contact is initially treated as open. After external intervention,
use the wall control to return the door fully closed to synchronize the controller.
Do not rely on its travel estimate as obstruction or motor feedback.

After installing, verify both auxiliary-input choices as applicable, Stop during
opening, a repeated Close while closing, warning cancellation followed by another
Close, and a closed contact during the warning. These require physical acceptance
testing in addition to the automated tests.

Dry-contact host checks (ESPHome `2026.8.1`, Python, and `g++` required):

```sh
python tests/test_dry_contact_commands.py
python tests/test_dry_contact_auxiliary.py
```

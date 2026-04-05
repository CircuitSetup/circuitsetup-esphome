# CircuitSetup ESPHome

ESPHome configurations and a custom `secplus_gdo` external component for CircuitSetup garage door opener hardware.
This repository builds on the original garage door opener work from [Konnected Inc.](https://github.com/konnected-io/konnected-esphome), whose earlier ESPHome support helped establish the foundation for this project.

## Choose a Starting Point

Use the repo in one of these three ways:

1. [`circuitsetup-gdo-default.yaml`](circuitsetup-gdo-default.yaml)
   The simplest starter/import path for CircuitSetup Security+ hardware.
2. [`circuitsetup-secplus-garage-door-opener.yaml`](circuitsetup-secplus-garage-door-opener.yaml)
   The full editable reference config for the Security+ opener.
3. [`components/secplus_gdo/README.md`](components/secplus_gdo/README.md)
   The advanced path for direct `external_components` usage and fully custom YAML.

For most users, start with the default or full Security+ YAML and only edit the documented substitutions near the top of the file.

## Quick Start

For CircuitSetup Security+ hardware, the quickest starting point is:

```yaml
packages:
  circuitsetup.secplus-garage-door-opener: github://CircuitSetup/circuitsetup-esphome/circuitsetup-gdo-default.yaml@master
```

That starter file pulls in the full Security+ package bundle, while the package bundle itself stays pinned to a tested release tag.

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

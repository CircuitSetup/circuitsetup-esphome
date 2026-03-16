# CircuitSetup ESPHome

ESPHome configurations and a custom `secplus_gdo` external component for CircuitSetup garage door opener hardware.

## Quick Start

For CircuitSetup Security+ hardware, the simplest starting point is:

```yaml
packages:
  circuitsetup.secplus-garage-door-opener: github://CircuitSetup/circuitsetup-esphome/circuitsetup-secplus-garage-door-opener.yaml@circuitsetup-secplus-garage-door-opener-v1.4.5-esphome-v2026.2.4
```

That package targets ESPHome `2026.2.0+`, imports the `secplus_gdo` component, and includes the standard light, cover, lock, warning, Wi-Fi, and diagnostics setup from a pinned release tag instead of a floating branch.

## Security Notes

For Home Assistant 2026.2+ deployments, the examples in this repo now document the recommended secrets-based security settings:

```yaml
api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password
```

## Advanced Usage

If you want to use the external component directly instead of the full package bundle, see [components/secplus_gdo/README.md](components/secplus_gdo/README.md).

The component now starts itself during setup, so custom configs no longer need a hidden `wifi.on_connect` hook to call `id(cs_gdo).start_gdo()`.

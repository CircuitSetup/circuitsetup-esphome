# CircuitSetup ESPHome Garage Door Configs

ESPHome configurations and external components for CircuitSetup garage door hardware, including the Security+ opener integration (`secplus_gdo`).

## Highlights in this update

- `gdolib` is now sourced directly from `https://github.com/CircuitSetup/gdolib` (pinned by `components/secplus_gdo/gdolib_ref.txt`).
- No `platformio_options` are required for the normal Security+ path.
- Core operations are now declarative via real component actions and buttons:
  - Sync/resync opener
  - Restart GDO comms
  - Reset learned travel timings
  - Door/light/lock toggle operations
- Added richer diagnostics (`text_sensor` + paired-device counters).
- Added compile-checked examples and GitHub Actions CI for ESPHome `2026.2.x`.

## Quick start (minimal)

Use `examples/minimal.yaml` directly, or adapt this pattern:

```yaml
external_components:
  - source:
      type: local
      path: ./components
    components: [secplus_gdo]

packages:
  core: !include ./packages/core-esp32-s3.yaml
  network: !include ./packages/wifi-esp32.yaml
  secplus: !include ./packages/gdo/base.yaml
```

Then run:

```bash
esphome config examples/minimal.yaml
esphome compile examples/minimal.yaml
```

## Advanced configuration

`examples/advanced.yaml` demonstrates substitution-based optional package switching. Optional packages can be disabled by setting a package substitution to:

```yaml
../packages/options/disabled.yaml
```

Useful optional packages:

- `../packages/gdo/diagnostics.yaml`
- `../packages/gdo/pre-close-warning.yaml`
- `../packages/warning-led.yaml`
- `../packages/buzzer-rtttl.yaml`
- `../packages/pre-close-warning-tones.yaml`
- `../packages/temp-sensor.yaml`
- `../packages/debug.yaml`

## `secplus_gdo` component usage

Create the parent component:

```yaml
secplus_gdo:
  id: cs_gdo
  input_gdo_pin: GPIO2
  output_gdo_pin: GPIO1
```

Child platforms reference it with `secplus_gdo_id: cs_gdo`.

### Declarative actions

Available automation actions:

- `secplus_gdo.sync`
- `secplus_gdo.resync`
- `secplus_gdo.restart_comms`
- `secplus_gdo.reset_travel_timings`
- `secplus_gdo.door_open`
- `secplus_gdo.door_close`
- `secplus_gdo.door_stop`
- `secplus_gdo.door_toggle`
- `secplus_gdo.light_toggle`
- `secplus_gdo.lock_toggle`

### Command buttons

`button` platform `secplus_gdo` supports `type` values:

- `sync`, `resync`, `restart_comms`, `reset_travel_timings`
- `door_open`, `door_close`, `door_stop`, `door_toggle`
- `light_toggle`, `lock_toggle`

## Troubleshooting

- **Not synced after boot/reconnect**: use the `Resync opener` button (`type: resync`) and verify wiring to opener terminals.
- **Frequent comm restarts**: check UART wiring/pin mappings and power stability.
- **No pre-close warning behavior**: include `packages/gdo/pre-close-warning.yaml` and warning packages (`warning-led`, `buzzer-rtttl`, `pre-close-warning-tones`).
- **Door/light/lock controls unavailable**: confirm `sync` binary sensor is `on` and protocol select is correct.

## Migration notes

### Removed user-level PlatformIO requirements

Old configs often contained:

```yaml
esphome:
  platformio_options:
    lib_deps:
      - https://github.com/CircuitSetup/gdolib
```

This is no longer needed. Remove it.

### Re-sync lambda replaced

Old re-sync flows used manual lambdas and number writes. Use:

- `button` platform `secplus_gdo` with `type: resync`, or
- action `secplus_gdo.resync`.

### Package refactor

`packages/secplus-gdo.yaml` is now a compatibility shim that includes modular packages in `packages/gdo/`.

## CI

GitHub Actions workflow `.github/workflows/esphome-ci.yml` runs:

- `esphome config` for both examples
- `esphome compile` for both examples

Pinned to ESPHome `2026.2.x`.

`gdolib` updates are controlled by `components/secplus_gdo/gdolib_ref.txt`.

# secplus_gdo

ESPHome external component for Security+ garage door openers.

## Base configuration

```yaml
secplus_gdo:
  id: cs_gdo
  input_gdo_pin: GPIO2
  output_gdo_pin: GPIO1
```

Optional tuning:

```yaml
secplus_gdo:
  id: cs_gdo
  input_gdo_pin: GPIO2
  output_gdo_pin: GPIO1
  invert_uart: true
  obstruction_from_status: true
  min_command_interval: 50ms
  sync_retry_interval: 30s
```

## Platforms

- `cover` (`platform: secplus_gdo`)
- `light` (`platform: secplus_gdo`)
- `lock` (`platform: secplus_gdo`)
- `binary_sensor` (`platform: secplus_gdo`, `type: ...`)
- `sensor` (`platform: secplus_gdo`, `type: ...`)
- `text_sensor` (`platform: secplus_gdo`, `type: ...`)
- `switch` (`platform: secplus_gdo`, `type: learn|toggle_only`)
- `number` (`platform: secplus_gdo`, `type: open_duration|close_duration|client_id|rolling_code`)
- `select` (`platform: secplus_gdo`, protocol selector)
- `button` (`platform: secplus_gdo`, command buttons)

## Actions

Use without lambdas:

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

Example:

```yaml
on_...:
  then:
    - secplus_gdo.resync: cs_gdo
```

## Button types

`button` platform `secplus_gdo` `type` options:

- `sync`, `resync`, `restart_comms`, `reset_travel_timings`
- `door_open`, `door_close`, `door_stop`, `door_toggle`
- `light_toggle`, `lock_toggle`

## Notes

- `gdolib` is provided in YAML via `esphome.platformio_options.lib_deps` (see `packages/gdo/base.yaml`).
- If you are not using this package, add `https://github.com/CircuitSetup/gdolib.git` to your YAML `lib_deps`.

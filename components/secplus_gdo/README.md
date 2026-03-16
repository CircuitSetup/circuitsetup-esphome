# secplus_gdo

ESPHome `2026.2.0+` custom component for Security+ garage door openers.

## Minimal Working Example

```yaml
external_components:
  - source: github://CircuitSetup/circuitsetup-esphome@circuitsetup-secplus-garage-door-opener-v1.4.5-esphome-v2026.2.4
    components: [secplus_gdo]

secplus_gdo:
  id: cs_gdo
  input_gdo_pin: GPIO2
  output_gdo_pin: GPIO1

cover:
  - platform: secplus_gdo
    id: gdo_door
    name: Garage Door
    device_class: garage
    secplus_gdo_id: cs_gdo
    pre_close_warning_duration: 5s

light:
  - platform: secplus_gdo
    id: gdo_light
    name: Garage Light
    secplus_gdo_id: cs_gdo

lock:
  - platform: secplus_gdo
    id: garage_lock_entity
    name: Lock
    secplus_gdo_id: cs_gdo

binary_sensor:
  - platform: secplus_gdo
    id: gdo_motion
    name: Motion
    type: motion
    secplus_gdo_id: cs_gdo
```

The component now starts automatically during setup. You do not need a separate `wifi.on_connect` automation to call `id(cs_gdo).start_gdo()`.

## Entity Types

`binary_sensor` types:
- `motion`
- `obstruction`
- `motor`
- `button`
- `sync`
- `wireless_remote`

`sensor` types:
- `openings`
- `paired_devices_total`
- `paired_devices_remotes`
- `paired_devices_keypads`
- `paired_devices_wall_controls`
- `paired_devices_accessories`

`text_sensor` types:
- `battery`

`number` types:
- `open_duration`
- `close_duration`
- `client_id`
- `rolling_code`

`switch` types:
- `learn`
- `toggle_only`

## Cover Options

- `secplus_gdo_id`: required parent component ID.
- `pre_close_warning_duration`: optional warning delay before close commands.
- `pre_close_warning_start`: optional automation that runs when the warning starts.
- `pre_close_warning_end`: optional automation that runs when the warning ends or is cancelled.

`toggle_only` behavior is still supported through the dedicated `switch` entity. That mode is useful for openers that only accept toggle commands instead of discrete open/close commands.

## Full Package Example

For the complete CircuitSetup hardware package, see [packages/secplus-gdo.yaml](https://github.com/CircuitSetup/circuitsetup-esphome/blob/master/packages/secplus-gdo.yaml).

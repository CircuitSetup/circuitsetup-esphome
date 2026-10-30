# secplus_gdo

ESPHome `2026.2.0+` custom component for Security+ garage door openers on ESP32 with ESP-IDF.

## When To Use This Path

Use the full package YAML if you want the standard CircuitSetup hardware experience.

Use the direct `secplus_gdo` component path when you want to:

- Build a custom YAML from scratch
- Use the component on a non-standard config
- Edit the component locally during development

The component starts automatically during setup. You do not need a separate `wifi.on_connect` automation to call `id(cs_gdo).start_gdo()`.

## Minimal Working Example

```yaml
external_components:
  - source: github://CircuitSetup/circuitsetup-esphome@master
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

The UART pins must be distinct internal ESP32 GPIO numbers. Pin inversion, custom pin modes, and GPIO expanders are not supported by the driver.

## gdolib Dependency

`secplus_gdo` declares its tested gdolib release in `to_code()` in [`__init__.py`](__init__.py). ESPHome installs that dependency automatically; no YAML `lib_deps` entry is needed.

Legacy `gdolib=...` entries in `esphome.platformio_options.lib_deps` are automatically replaced with the component's tested dependency during validation. Both list and scalar forms are supported, and unrelated libraries are preserved. Your YAML file is not modified, so existing configurations need no manual edit.

For the first build with this dependency handling, ensure the external component cache has refreshed. You can temporarily set `refresh: 0s` on its `external_components` entry, then restore the normal interval after migrating.

Maintainers must update the Git tag in `GDOLIB_REPOSITORY` in `__init__.py` when adopting a new gdolib release. After ESPHome refreshes the external component, its next build uses the pin from that component. Publishing a gdolib release alone does not change the pin. Remote components have a default refresh interval of one day; see [ESPHome external-component refresh](https://esphome.io/components/external_components/#refresh).

The standalone component supports ESPHome `2026.2.0+`; the full CircuitSetup configuration requires `2026.6.0+`.

## Local Development Workflow

For local component edits, point `external_components` at the repo checkout instead of GitHub:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [secplus_gdo]
```

If you also need to edit package files such as `packages/secplus-gdo.yaml` or `packages/wifi-esp32.yaml`, use a local copy of the full top-level YAML instead of the remote package import.

To test a different gdolib release or fork, change `GDOLIB_REPOSITORY` in your local component copy.

## Supported Entity Types

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

The `select` platform configures the Security+ protocol (`auto`, Security+ 1.0, Security+ 2.0, or Security+ 1.0 with smart panel).

## Cover Options

- `secplus_gdo_id`: required parent component ID
- `pre_close_warning_duration`: optional warning delay before close commands
- `pre_close_warning_start`: optional automation that runs when the warning starts
- `pre_close_warning_end`: optional automation that runs when the warning ends or is cancelled

`toggle_only` behavior is still supported through the dedicated `switch` entity. That mode is useful for openers that only accept toggle commands instead of discrete open/close commands.

## Reserved IDs

The component validates generated ESPHome IDs against gdolib symbol names so generated C++ does not collide with the library.

Do not reuse IDs such as:

- `gdo_lock`
- `gdo_start`
- `gdo_sync`
- `gdo_set_rolling_code`
- `gdo_light_on`

If config validation reports a reserved ID, rename the ESPHome `id` to something project-specific such as `garage_lock_entity`, `shop_gdo_light`, or `cs_gdo`.

## Full Package Example

For the complete CircuitSetup hardware package, see [`packages/secplus-gdo.yaml`](https://github.com/CircuitSetup/circuitsetup-esphome/blob/master/packages/secplus-gdo.yaml).

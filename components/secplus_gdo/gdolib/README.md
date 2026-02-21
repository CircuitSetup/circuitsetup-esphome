# gdolib

Garage door opener library for controlling Security+ 1.0 and 2.0 garage door openers on ESP32.

## Version and compatibility
- Library version: `1.1.0-cs1` (`GDOLIB_VERSION_STRING`)
- Targeted for ESPHome `2026.2.x`
- Compatible with ESP-IDF `5.1+` (native and Arduino-ESP32 backends used by ESPHome)

## Local hardening in this repo
- Fixed TX packet leak on queue send failure and during deinit queue teardown.
- Fixed timer lifecycle leaks (`start_once` failure path, V1 sync timer cleanup, obstruction timer args lifecycle).
- Added safer start/deinit cleanup paths so failed startup does not leave UART/task resources behind.
- Added packet decode validation guard to ignore corrupt/noisy wireline frames.
- Added safer enum-to-string conversions (including battery status) to prevent out-of-bounds reads.
- Break generation now prefers official UART APIs and falls back to the prior deterministic method when unavailable.

## Replay test harness
A small deterministic secplus replay test is included at:
- `tests/secplus_replay_test.c`

It validates:
- wireline encode/decode round-trips
- framing rejection
- corrupted payload rejection

## Credits
This library incorporates secplus by Clayton Smith: https://github.com/argilo/secplus

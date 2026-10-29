# GDO Event Queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Deliver gdolib events on the main loop with bounded memory and overflow recovery.

**Architecture:** Keep existing event processing. Transfer copied status through
two ESPHome lock-free queues and fixed event slots; wake the component only
when needed. Refresh current state after dropped events.

**Tech Stack:** C++, ESPHome, ESP-IDF, Python/pytest host harnesses.

**Spec:** `docs/superpowers/specs/2026-10-29-gdo-event-queue-design.md`

## Global Constraints

- Leave gdolib and the existing `v1.3.3` dependency pin to the other task.
- Preserve direct component compatibility with ESPHome 2026.2.0+.
- No hardware operation, firmware release, or Konnected version changes.

## Review Focus

- Burst overflow must restore current states without replaying button/motor side effects.
- A lost sync event must still run existing rolling-code/diagnostic recovery.
- Failed status reads must leave refresh pending.
- An event arriving during the final loop disable must wake processing again.
- Existing diagnostic restart must retain calibration and wait for motion to stop.

### Task 1: Bounded event handoff

**Files:** Modify `components/secplus_gdo/secplus_gdo.{h,cpp}`,
`components/secplus_gdo/README.md`, and the affected callback/diagnostic tests.
Create `tests/test_secplus_gdo_event_queue.py` with a host runtime harness.
Update `tests/test_dry_contact_commands.py` to include the new upstream trigger
argument alias when present, so the existing harness also runs on ESPHome 2026.10.

**Interfaces:** Keep `defer_gdo_event(const gdo_status_t &, gdo_cb_event_t)` as
the callback entry point. Add `loop()`, `handle_gdo_event_()`, and
`republish_status_()`; the latter returns success so a failed read can retry.

- [x] Write and run failing host checks for ordered independent snapshots,
  overflow refresh, dropped sync recovery, failed reads, and final-disable wakeup.
- [x] Replace per-event defer with fixed slots and ESPHome queues, initialized
  before the driver starts. Move protocol tracking into the main-loop handler.
- [x] Refresh latched diagnostics without replaying button or motor actions;
  update existing tests to use the main-loop handler where appropriate.
- [x] Run host checks, the full suite in a compatible ESPHome environment,
  and a native firmware build if available; inspect diff and obtain review.
- [x] Commit the verified change on the feature branch.

## Verification and review

- Baseline: 33 tests and 31 subtests passed in the existing ESPHome 2026.9-dev image.
- Added regression checks failed against the previous scheduler handoff and passed
  after implementation. The harness uses the real ESPHome lock-free queue.
- Independent review found a dropped-sync/status-snapshot race. The new
  `test_new_sync_drop_during_refresh_uses_a_new_snapshot` reproduced it before
  the flag exchange was moved ahead of the status read; the test now passes.
- Final suites: 40 tests and 31 subtests passed on both ESPHome 2026.10.0 and
  2026.9-dev. The latter reports one existing AnyIO/pytest deprecation warning.
- All eight queue host checks also pass with the installed ESPHome 2026.2.1 headers.
- Full ESP32-S3 firmware compilation passed on ESPHome 2026.10.0, using local
  packages/components, including the web-server components, and the unchanged
  `CircuitSetup/gdolib#v1.3.3` pin. Generated sources matched the final change.
  Reported RAM: 139255/341760 bytes; flash: 1285895/1835008 bytes.
- No firmware was flashed; web `/events` stack usage and physical door behavior
  still require hardware validation.

## Follow-up release metadata

- User requested Security+ firmware **1.5.9 → 1.6.0**, gdolib **v1.4.0**, and a PR.
- Update the version and dependency in the Security+ YAML, and match the direct
  component README's pin. The full suite passes: 40 tests and 31 subtests.
- Gdolib PR #9 targets 1.4.0 at `cd49b090817b331c5df483b25e14ce6fb550a695`.
  Its release workflow publishes v1.4.0 after merge, so this firmware PR depends
  on that tag being available. Full ESP32-S3 firmware compilation passed on
  ESPHome 2026.10.0 against that immutable 1.4.0 candidate (RAM 139263/341760
  bytes; flash 1287283/1835008 bytes). The published tag remains pending.

## PR review corrections

- Persist nonzero travel durations during overflow refresh; otherwise a lost
  measurement never reaches preferences and disappears on reboot.
- Remember dropped motor-on events, consume their old button/cover attribution
  before publishing refreshed state, and retain that recovery across read errors.
  An unrelated overflow must preserve attribution for a command still pending.
- Keep tentative AUTO protocol values in internal tracking only; update the
  saved selector after completed sync, matching the normal event handler.
- After successful driver teardown, discard obsolete queued callbacks and
  rebuild all slots, including any abandoned by a terminated callback. Clear
  counters and recovery flags; leave queues alone when teardown fails.
- Each defect was reproduced before its fix. Host regressions now execute the
  real motor handler and the real queue cleanup/restart paths.
- Review fixes are P2 correctness issues; P3 issues are excluded at the user's request.
- Final verification: 48 tests and 34 subtests passed on ESPHome 2026.10.0;
  all 20 queue/restart host checks also passed with ESPHome 2026.2.1 headers.
  The final ESP32-S3 build passed against gdolib candidate `cd49b09` (RAM
  139263/341760 bytes; flash 1287639/1835008 bytes). No hardware was operated.

# Bounded GDO event delivery

Port the useful part of upstream `b9bc41e7b42ca3825a18ef3657365d8a6d64cb3c`:
bounded, allocation-free delivery from gdolib to ESPHome's main loop. The user
authorized implementation if worthwhile; gdolib changes belong to another task.
The motivating failure is [upstream issue #148](https://github.com/konnected-io/konnected-esphome/issues/148):
ESPHome 2026.10 web event serialization overflowed the 4 KB gdolib task stack.
The callback must avoid all ESPHome scheduler/publishing/preference work, since
increasing that stack alone does not establish task safety.

## Approach

Keep this fork's event processing, startup, diagnostic resync, rolling-code
recovery, and entities. Replace per-event scheduler callbacks with 31 fixed
event slots and ESPHome's single-producer/single-consumer lock-free queues.
One queue holds free slots; the other holds copied status snapshots. This
avoids upstream's dependency on the newer `EventPool::warm()` API and retains
the direct component's ESPHome 2026.2 support.

The alternatives are retaining allocating scheduler callbacks (unbounded
bursts) or copying upstream's pool implementation (newer ESPHome requirement).
Fixed storage is the implementation choice for this backport.

The callback only copies/queues events and requests the main loop. On overflow,
refresh current latched states, including this fork's extra diagnostics. A
dropped sync event must still run the existing sync recovery. Do not synthesize
button presses, remote activations, or duration measurements during refresh.
Retry a failed status read without losing the refresh request. Disable the loop
when idle, using ESPHome's pending wake request to cover enqueue/disable races.

No gdolib edit or version pin change, Konnected project version, release, flash,
or physical door operation is included. Host checks must cover snapshot order,
bounded overflow, recovery, and idle wakeup; build if the environment permits.

## Follow-up release metadata

The user subsequently requested a PR, firmware version **1.6.0**, and a gdolib
**v1.4.0** pin. This extends the original scope to the Security+ version and its
documented dependency. Gdolib source remains owned by the other task:
[gdolib PR #9](https://github.com/CircuitSetup/gdolib/pull/9). The firmware PR
must wait for that PR's merge and published v1.4.0 tag before merging.

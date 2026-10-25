# Dry-contact setup and calibration

This guide applies to [`circuitsetup-gdo-dry-contact.yaml`](../circuitsetup-gdo-dry-contact.yaml), the separate dry-contact firmware. For Security+ or Security+ 2.0 installation, return to the [main installation guide](../README.md).

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

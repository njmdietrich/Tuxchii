# Switch 2 Pro Controller USB HD rumble

`feature/switch2-pro-usb-hd-rumble` adds an opt-in experimental output path for a
physical Nintendo Switch 2 Pro Controller (`057e:2069`) connected by USB.

In **Configure > Controls > Vibration: Configure**, enable **Switch 2 Pro USB HD
Rumble (experimental)**. The feature keeps each emulated side's low and high bands
on its corresponding physical actuator. Player vibration strength still applies;
the ordinary two-motor frequency mapping is bypassed. Other controller models and
Bluetooth continue to use their existing paths.

The log reports `Switch 2 Pro USB HD rumble: experimental frequency translation
enabled` when the patched backend accepts the capability query. A stock/system SDL
without that backend falls back to ordinary rumble. If Steam Input replaces the
physical controller with a virtual Xbox controller, select the physical controller
directly for this feature.

## Frequency calibration remains experimental

The USB report format and SDL's neutral frequency codes are known, but the exact
Hz-to-code relationship has not been measured here. This version adopts the
experimental logarithmic heuristic from
[switch2-pico's codec](https://github.com/joshbirnholz/switch2-pico/blob/e0223bebcf1812ec71d33e1d5d09bb1d9f6f9808/core/src/hd_rumble.c):
160 Hz maps to `0x112`, 320 Hz maps to `0x187`, and the slope is 117 codes per octave.
These are assumed Hz anchors, not verified frequency measurements. The option is
off by default until hardware testing establishes how the translated effects feel.
This is not a claim of bit-exact native HD Rumble 2 reproduction.

Amplitudes are linear and bounded to a shared ceiling of 450 per actuator, below
SDL's existing Switch 2 limit. Both bands retain their relative strengths when
their sum reaches that ceiling. Non-finite amplitudes become silence; invalid
frequencies use the neutral codes.

## Transport and timing

The SDL HIDAPI driver owns the controller connection. No second HID handle is
opened, and motion/input initialization remains SDL's responsibility. The private
`LCH2HD01` effect carries two independent high-frequency/amplitude and
low-frequency/amplitude tuples with little-endian 16-bit fields and a remaining
duration per side. Its eight-byte header alone queries support without sending a
controller report. A full effect is 28 bytes: header, then two ten-byte sides.

SDL emits USB output report `0x02`, with a sequence header and five-byte packed
sample at each actuator's sixteen-byte block. Refreshes use a four-millisecond
interval, as in the Linux Switch 2 driver. Stops are explicit; each side expires
independently after at most two seconds, matching the ordinary rumble timeout.
Updating the other side cannot extend an old effect. Closing the controller sends
a final stop, and ordinary rumble takes over if the custom path is disabled or fails.

Streamed backends bypass the frontend's fifteen-millisecond throttle. Repeated
nonzero samples also reach streamed backends so their watchdogs can stay alive
during constant effects. Duplicate silence is still suppressed.

## Building and checking

Configure with `-DENABLE_SWITCH2_HD_RUMBLE=ON` to build the same SDL commit as the
existing prebuilt package (`147a8ee32d`), pinned by its SHA-512 archive hash, with
`.patch/sdl3-hd-rumble/0001-switch2-pro-usb-hd-rumble.patch`. This takes precedence
over the prebuilt SDL option. The Linux x86-64 PGO workflow enables it, installs the
SDL X11/Wayland/audio development dependencies, and runs the backend tests before
building the emulator. FSR4 packaging is retained.

The host tests in `src/tests/input_common/switch2_rumble.cpp` cover routing, field
encoding, malformed values, limits and independent expiry. To exercise the actual
patched SDL parser, report encoder and scheduling functions with a fake USB writer:

```sh
python3 tools/input/test_switch2_sdl_patch.py --sdl-source /path/to/patched/SDL --sanitize
```

The harness uses SDL's production functions with a simulated clock; it does not
test a connected controller. Initial hardware checks should verify left/right
isolation, short effects, sustained effects, stopping, unplug/reconnect and motion
input while rumbling. Compare a repeatable game action against the ordinary path.
Frequency sweeps measured on an actuator are needed before the provisional Hz
mapping can be treated as calibrated.

## Sources

- [SDL Switch 2 driver](https://github.com/libsdl-org/SDL/blob/147a8ee32d/src/joystick/hidapi/SDL_hidapi_switch2.c)
- [Switch 2 controller report research](https://github.com/ndeadly/switch2_controller_research/blob/master/hid_reports.md#output-report-0x02)
- [Linux Switch 2 rumble driver](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c)
- [switch2-pico](https://github.com/joshbirnholz/switch2-pico), MIT; notice included in
  `dist/licenses/switch2-pico.txt` and the Linux packages.

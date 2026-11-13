#!/usr/bin/env python3
"""Exercise the actual patched SDL rumble functions with a fake USB writer and clock."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^static (?:bool|void|Uint16) " + name + r"\([^;]*?\)\s*\{",
                      source, re.MULTILINE | re.DOTALL)
    if not match:
        raise ValueError(f"Cannot find SDL function {name}")
    start = source.index("{", match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PREAMBLE = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef struct { int unused; } SDL_Joystick;
typedef struct { bool is_bluetooth; Uint16 product_id; void *context, *parent; } SDL_HIDAPI_Device;
#define USB_PRODUCT_NINTENDO_SWITCH2_PRO 0x2069
#define USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER 0x2073
#define USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_LEFT 0x2067
#define USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_RIGHT 0x2066
#define RUMBLE_MAX 29000
#define RUMBLE_INTERVAL 12
#define SDL_min(a,b) ((a) < (b) ? (a) : (b))
#define SDL_max(a,b) ((a) > (b) ? (a) : (b))
#define SDL_clamp(x,a,b) SDL_min(SDL_max(x,a),b)
#define SDL_memcmp memcmp
#define SDL_memcpy memcpy
#define SDL_zero(x) memset(&(x), 0, sizeof(x))
#define SDL_RemoveHintCallback(...) ((void)0)
static unsigned checks, writes;
static Uint64 ticks;
static bool lock_ok = true, send_ok = true;
static Uint8 last_report[64];
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static Uint64 SDL_GetTicks(void) { return ticks; }
static bool SDL_Unsupported(void) { return false; }
static bool SDL_SetError(const char *format, ...) { (void)format; return false; }
static bool SDL_HIDAPI_LockRumble(void) { return lock_ok; }
static int SDL_HIDAPI_SendRumbleAndUnlock(SDL_HIDAPI_Device *device, const void *data, int size) {
    (void)device;
    if (!send_ok) return -1;
    CHECK(size == 64);
    memcpy(last_report, data, 64);
    ++writes;
    return size;
}
"""

TESTS = r"""
static Uint16 amplitude(int side, bool high) {
    const Uint8 *data = last_report + 2 + side * 16;
    return high ? (Uint16)((data[1] >> 2) | ((data[2] & 15) << 6))
                : (Uint16)((data[3] >> 6) | (data[4] << 2));
}
static void u16(Uint8 *data, Uint16 value) { data[0] = value & 255; data[1] = value >> 8; }
int main(void) {
    SDL_HIDAPI_Device device = {.product_id = USB_PRODUCT_NINTENDO_SWITCH2_PRO};
    SDL_DriverSwitch2_Context ctx = {.device = &device};
    device.context = &ctx;
    Uint8 effect[28] = {'L','C','H','2','H','D','0','1',
        0x87,1,0,0,0x12,1,0xc2,1,0xd0,7,
        0x12,1,100,0,0x87,1,0,0,0xf4,1};
    ticks = 1000;

    /* Query and malformed packets must not send or mutate controller state. */
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 8));
    CHECK(writes == 0 && !ctx.hd_rumble_enabled);
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, NULL, 8));
    for (int size = 0; size <= 28; ++size) {
        if (size != 8 && size != 28)
            CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, size));
    }
    effect[0] = 'X';
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    effect[0] = 'L';
    device.is_bluetooth = true;
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 8));
    device.is_bluetooth = false;
    device.product_id = USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER;
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 8));
    device.product_id = USB_PRODUCT_NINTENDO_SWITCH2_PRO;
    CHECK(writes == 0 && !ctx.hd_rumble_enabled);

    /* The host ABI must produce independently packed left and right USB blocks. */
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    const Uint8 left[5] = {0x87, 0x01, 0x20, 0x91, 0x70};
    const Uint8 right[5] = {0x12, 0x91, 0x71, 0x18, 0x00};
    CHECK(writes == 1 && last_report[0] == 2);
    CHECK(last_report[1] == 0x50 && last_report[17] == 0x50);
    CHECK(memcmp(last_report + 2, left, 5) == 0);
    CHECK(memcmp(last_report + 18, right, 5) == 0);
    CHECK(amplitude(0, false) == 450 && amplitude(1, true) == 100);
    for (int i = 7; i < 17; ++i) CHECK(last_report[i] == 0);
    for (int i = 23; i < 64; ++i) CHECK(last_report[i] == 0);

    /* Refresh is paced; expiry on one side cannot stop the other side. */
    ticks = 1001; CHECK(UpdateRumble(&ctx)); CHECK(writes == 1);
    ticks = 1004; CHECK(UpdateRumble(&ctx)); CHECK(writes == 2);
    ticks = 1500; CHECK(UpdateRumble(&ctx));
    CHECK(amplitude(0, false) == 450 && amplitude(1, true) == 0);
    ticks = 3000; CHECK(UpdateRumble(&ctx));
    CHECK(amplitude(0, false) == 0 && amplitude(1, true) == 0);
    unsigned stopped_writes = writes;
    ticks = 3004; CHECK(UpdateRumble(&ctx)); CHECK(writes == stopped_writes);

    /* A hostile amplitude or timeout is bounded even if the host encoder is bypassed. */
    u16(effect + 10, 65535); u16(effect + 14, 65535); u16(effect + 16, 65535);
    ticks = 4000;
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    CHECK(amplitude(0, true) + amplitude(0, false) <= (RUMBLE_MAX >> 6));
    CHECK(ctx.hd_rumble[0].expires == 6000);

    /* Explicit stops, standard-rumble takeover and close all reach the wire. */
    u16(effect + 16, 0); u16(effect + 26, 0);
    ticks = 4004;
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    CHECK(amplitude(0, true) == 0 && amplitude(0, false) == 0);
    CHECK(amplitude(1, true) == 0 && amplitude(1, false) == 0);
    u16(effect + 16, 1000);
    ticks = 5000;
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    CHECK(HIDAPI_DriverSwitch2_RumbleJoystick(&device, NULL, 0, 0));
    CHECK(!ctx.hd_rumble_enabled);
    CHECK(UpdateRumble(&ctx));
    CHECK(amplitude(0, true) == 0 && amplitude(0, false) == 0);
    ticks = 6000;
    CHECK(HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    HIDAPI_DriverSwitch2_CloseJoystick(&device, NULL);
    CHECK(!ctx.hd_rumble_enabled);
    CHECK(amplitude(0, true) == 0 && amplitude(0, false) == 0);

    /* A transient writer failure leaves the latest state pending for retry. */
    ticks = 7000; send_ok = false;
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    CHECK(ctx.rumble_updated);
    ticks = 7004; send_ok = true;
    CHECK(UpdateRumble(&ctx)); CHECK(!ctx.rumble_updated);
    CHECK(amplitude(0, true) + amplitude(0, false) > 0);
    ticks = 7008; lock_ok = false;
    unsigned before_lock = writes;
    CHECK(!HIDAPI_DriverSwitch2_SendJoystickEffect(&device, NULL, effect, 28));
    CHECK(writes == before_lock && ctx.rumble_updated);
    lock_ok = true;
    CHECK(UpdateRumble(&ctx));
    CHECK(writes == before_lock + 1 && !ctx.rumble_updated);
    for (int i = 0; i < 20; ++i) {
        unsigned sequence = ctx.rumble_seq & 15;
        ticks += 4; CHECK(UpdateRumble(&ctx));
        CHECK(last_report[1] == (0x50 | sequence));
        CHECK(last_report[17] == last_report[1]);
    }
    CHECK(HIDAPI_DriverSwitch2_RumbleJoystick(&device, NULL, 65535, 32768));
    ticks += 4; CHECK(UpdateRumble(&ctx));
    CHECK(!ctx.hd_rumble_enabled);
    CHECK(amplitude(0, true) == amplitude(1, true));
    CHECK(amplitude(0, false) == amplitude(1, false));
    CHECK(amplitude(0, true) > 0 && amplitude(0, false) > 0);
    printf("Switch 2 SDL backend: %u checks passed\n", checks);
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdl-source", type=Path)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    if args.sdl_source is None:
        root = Path(__file__).resolve().parents[2]
        version = json.loads((root / "cpmfile.json").read_text())["sdl3-hd-rumble"]["version"]
        args.sdl_source = root / ".cache/cpm/sdl3" / version
    source = (args.sdl_source / "src/joystick/hidapi/SDL_hidapi_switch2.c").read_text()
    fields = source[source.index("    Uint64 rumble_timestamp;"):
                    source.index("    Switch2_StickCalibration left_stick;")]
    context = ("typedef struct { SDL_HIDAPI_Device *device; SDL_Joystick *joystick;\n" +
               fields + "} SDL_DriverSwitch2_Context;\n")
    names = ["ReadHDValue", "EncodeHDRumble", "UpdateRumble",
             "HIDAPI_DriverSwitch2_SendJoystickEffect", "HIDAPI_DriverSwitch2_RumbleJoystick",
             "HIDAPI_DriverSwitch2_CloseJoystick"]
    harness = PREAMBLE + context + "\n".join(function(source, name) for name in names) + TESTS
    with tempfile.TemporaryDirectory(prefix="switch2-sdl-test-") as directory:
        test = Path(directory) / "test.c"
        binary = Path(directory) / "test"
        test.write_text(harness)
        flags = ["-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Werror"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run([args.cc, *flags, str(test), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()

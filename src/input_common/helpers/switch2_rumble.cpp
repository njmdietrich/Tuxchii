// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 switch2-pico contributors

#include <algorithm>
#include <cmath>

#include "input_common/helpers/switch2_rumble.h"

namespace InputCommon::Switch2Rumble {
namespace {

u16 Frequency(f32 hz, u16 neutral) {
    if (!std::isfinite(hz) || hz <= 0.0f) {
        return neutral;
    }
    // Experimental scale from joshbirnholz/switch2-pico's hd_rumble.c (ed9f80e0a8):
    // anchor 160/320 Hz to the console's 0x112/0x187 neutral codes. The slope has
    // not been measured on hardware; this is not a verified Hz-to-code conversion.
    const f32 code = 274.0f + 117.0f * std::log2(hz / 160.0f);
    return static_cast<u16>(std::lround(std::clamp(code, 1.0f, 1023.0f)));
}

f32 Amplitude(f32 value) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

void WriteU16(Effect& effect, size_t offset, u16 value) {
    effect[offset] = static_cast<u8>(value & 0xff);
    effect[offset + 1] = static_cast<u8>(value >> 8);
}

} // namespace

bool IsSupportedController(u16 vendor, u16 product, bool wired) {
    return vendor == 0x057e && product == 0x2069 && wired;
}

Sample Encode(const Common::Input::VibrationStatus& vibration) {
    const f32 low = Amplitude(vibration.low_amplitude);
    const f32 high = Amplitude(vibration.high_amplitude);
    const f32 scale = static_cast<f32>(MaxAmplitude) / (std::max)(1.0f, low + high);
    return {
        .high_frequency = Frequency(vibration.high_frequency, 0x187),
        .high_amplitude = static_cast<u16>(high * scale),
        .low_frequency = Frequency(vibration.low_frequency, 0x112),
        .low_amplitude = static_cast<u16>(low * scale),
    };
}

void State::Update(const Common::Input::VibrationStatus& vibration, u64 now_ms) {
    const Side side{Encode(vibration), now_ms};
    if (vibration.side != Common::Input::VibrationSide::Right) {
        sides[0] = side;
    }
    if (vibration.side != Common::Input::VibrationSide::Left) {
        sides[1] = side;
    }
}

Effect State::MakeEffect(u64 now_ms) const {
    Effect effect{};
    std::copy(EffectMagic.begin(), EffectMagic.end(), effect.begin());
    for (size_t index = 0; index < sides.size(); ++index) {
        const Side& side = sides[index];
        const u64 elapsed = now_ms >= side.updated_ms ? now_ms - side.updated_ms : TimeoutMs;
        const u16 remaining = static_cast<u16>(elapsed < TimeoutMs ? TimeoutMs - elapsed : 0);
        const bool active = remaining != 0;
        const size_t offset = EffectMagic.size() + index * 10;
        WriteU16(effect, offset, side.sample.high_frequency);
        WriteU16(effect, offset + 2, active ? side.sample.high_amplitude : 0);
        WriteU16(effect, offset + 4, side.sample.low_frequency);
        WriteU16(effect, offset + 6, active ? side.sample.low_amplitude : 0);
        WriteU16(effect, offset + 8,
                 (side.sample.high_amplitude || side.sample.low_amplitude) ? remaining : 0);
    }
    return effect;
}

} // namespace InputCommon::Switch2Rumble

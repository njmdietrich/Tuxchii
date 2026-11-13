// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"
#include "common/input.h"

namespace InputCommon::Switch2Rumble {

// Private SDL effect ABI; the eight-byte form queries support without playing anything.
inline constexpr std::array<u8, 8> EffectMagic{'L', 'C', 'H', '2', 'H', 'D', '0', '1'};
inline constexpr u64 TimeoutMs = 2000;
inline constexpr u16 MaxAmplitude = 450;
using Effect = std::array<u8, 28>;

bool IsSupportedController(u16 vendor, u16 product, bool wired);

struct Sample {
    u16 high_frequency{0x187};
    u16 high_amplitude{};
    u16 low_frequency{0x112};
    u16 low_amplitude{};
};

Sample Encode(const Common::Input::VibrationStatus& vibration);

class State {
public:
    void Update(const Common::Input::VibrationStatus& vibration, u64 now_ms);
    [[nodiscard]] Effect MakeEffect(u64 now_ms) const;

private:
    struct Side {
        Sample sample;
        u64 updated_ms{};
    };
    std::array<Side, 2> sides{};
};

} // namespace InputCommon::Switch2Rumble

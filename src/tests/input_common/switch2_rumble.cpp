// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "input_common/helpers/switch2_rumble.h"

namespace {
namespace Rumble = InputCommon::Switch2Rumble;
using Common::Input::VibrationSide;

Common::Input::VibrationStatus Vibration(VibrationSide side, f32 low, f32 high) {
    return {.low_amplitude = low,
            .low_frequency = 160.0f,
            .high_amplitude = high,
            .high_frequency = 320.0f,
            .type = Common::Input::VibrationAmplificationType::Linear,
            .side = side};
}

u16 Read(const Rumble::Effect& effect, size_t offset) {
    return static_cast<u16>(effect[offset] | (static_cast<u16>(effect[offset + 1]) << 8));
}
} // namespace

TEST_CASE("Switch 2 HD rumble selects only the physical USB Pro Controller", "[input_common]") {
    CHECK(Rumble::IsSupportedController(0x057e, 0x2069, true));
    CHECK_FALSE(Rumble::IsSupportedController(0x057e, 0x2069, false));
    CHECK_FALSE(Rumble::IsSupportedController(0x057e, 0x2009, true));
    CHECK_FALSE(Rumble::IsSupportedController(0x057e, 0x2066, true));
    CHECK_FALSE(Rumble::IsSupportedController(0x054c, 0x2069, true));
}

TEST_CASE("Experimental Switch 2 frequency scale retains the neutral anchors", "[input_common]") {
    auto vibration = Vibration(VibrationSide::Both, 0.5f, 0.5f);
    const auto neutral = Rumble::Encode(vibration);
    CHECK(neutral.low_frequency == 0x112);
    CHECK(neutral.high_frequency == 0x187);
    vibration.low_frequency = 80.0f;
    vibration.high_frequency = 640.0f;
    const auto changed = Rumble::Encode(vibration);
    CHECK(changed.low_frequency < neutral.low_frequency);
    CHECK(changed.high_frequency > neutral.high_frequency);
    CHECK(changed.low_amplitude == neutral.low_amplitude);
    CHECK(changed.high_amplitude == neutral.high_amplitude);
}

TEST_CASE("Switch 2 invalid vibration values become bounded output", "[input_common]") {
    auto vibration = Vibration(VibrationSide::Both, -1.0f,
                               std::numeric_limits<f32>::quiet_NaN());
    vibration.low_frequency = std::numeric_limits<f32>::infinity();
    vibration.high_frequency = 0.0f;
    const auto invalid = Rumble::Encode(vibration);
    CHECK(invalid.low_amplitude == 0);
    CHECK(invalid.high_amplitude == 0);
    CHECK(invalid.low_frequency == 0x112);
    CHECK(invalid.high_frequency == 0x187);
    vibration.low_frequency = std::numeric_limits<f32>::min();
    vibration.high_frequency = std::numeric_limits<f32>::max();
    vibration.high_amplitude = 100.0f;
    const auto extreme = Rumble::Encode(vibration);
    CHECK(extreme.low_frequency == 1);
    CHECK(extreme.high_frequency == 1023);
    CHECK(extreme.high_amplitude == Rumble::MaxAmplitude);
}

TEST_CASE("Switch 2 bands share the amplitude ceiling without losing their ratio", "[input_common]") {
    const auto both = Rumble::Encode(Vibration(VibrationSide::Both, 1.0f, 1.0f));
    CHECK(both.low_amplitude == 225);
    CHECK(both.high_amplitude == 225);
    const auto unequal = Rumble::Encode(Vibration(VibrationSide::Both, 1.0f, 0.5f));
    CHECK(unequal.low_amplitude == 300);
    CHECK(unequal.high_amplitude == 150);
    CHECK(Rumble::Encode(Vibration(VibrationSide::Both, 0.0f, 0.0f)).high_amplitude == 0);
}

TEST_CASE("Switch 2 effect ABI carries the expected little endian fields", "[input_common]") {
    Rumble::State state;
    state.Update(Vibration(VibrationSide::Left, 1.0f, 0.0f), 1000);
    const Rumble::Effect expected{
        'L', 'C', 'H', '2', 'H', 'D', '0', '1',
        0x87, 0x01, 0, 0, 0x12, 0x01, 0xc2, 0x01, 0xd0, 0x07,
        0x87, 0x01, 0, 0, 0x12, 0x01, 0, 0, 0, 0};
    CHECK(state.MakeEffect(1000) == expected);
}

TEST_CASE("Stopping one Switch 2 actuator leaves the other actuator alone", "[input_common]") {
    Rumble::State state;
    state.Update(Vibration(VibrationSide::Left, 0.5f, 0.0f), 1000);
    state.Update(Vibration(VibrationSide::Right, 0.0f, 1.0f), 1010);
    const auto stereo = state.MakeEffect(1010);
    CHECK(Read(stereo, 14) == 225);
    CHECK(Read(stereo, 20) == 450);
    state.Update(Vibration(VibrationSide::Right, 0.0f, 0.0f), 1020);
    const auto stopped = state.MakeEffect(1020);
    CHECK(Read(stopped, 14) == 225);
    CHECK(Read(stopped, 16) == 1980);
    CHECK(Read(stopped, 20) == 0);
    CHECK(Read(stopped, 26) == 0);
}

TEST_CASE("Switch 2 side updates cannot extend the other side's watchdog", "[input_common]") {
    Rumble::State state;
    state.Update(Vibration(VibrationSide::Left, 1.0f, 0.0f), 1000);
    state.Update(Vibration(VibrationSide::Right, 0.0f, 1.0f), 2000);
    state.Update(Vibration(VibrationSide::Right, 0.0f, 0.5f), 3001);
    const auto expired = state.MakeEffect(3001);
    CHECK(Read(expired, 14) == 0);
    CHECK(Read(expired, 16) == 0);
    CHECK(Read(expired, 20) == 225);
    CHECK(Read(expired, 26) == 2000);
    CHECK(Read(state.MakeEffect(5001), 20) == 0);
    CHECK(Read(state.MakeEffect(10), 20) == 0);
}

TEST_CASE("Switch 2 both-side effects and independent controllers retain their routing",
          "[input_common]") {
    Rumble::State first, second;
    first.Update(Vibration(VibrationSide::Both, 0.25f, 0.5f), 1000);
    const auto effect = first.MakeEffect(1000);
    CHECK(Read(effect, 10) == Read(effect, 20));
    CHECK(Read(effect, 14) == Read(effect, 24));
    CHECK(Read(second.MakeEffect(1000), 10) == 0);
    CHECK(Read(second.MakeEffect(1000), 20) == 0);
    first.Update(Vibration(VibrationSide::Both, 0.0f, 0.0f), 1010);
    CHECK(Read(first.MakeEffect(1010), 10) == 0);
    CHECK(Read(first.MakeEffect(1010), 20) == 0);
}

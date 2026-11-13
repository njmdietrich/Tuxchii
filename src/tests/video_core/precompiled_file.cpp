// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "video_core/precompiled_file.h"

namespace {

struct TempDir {
    TempDir() {
        path = std::filesystem::temp_directory_path() /
               ("precompiled-file-" + std::to_string(std::random_device{}()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

std::vector<u8> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

using VideoCommon::SavePrecompiledFile;
using VideoCommon::SavePrecompiledResult;

} // namespace

TEST_CASE("Reselecting an identical recipe preserves the preparation timestamp",
          "[video_core][precompiled_file]") {
    TempDir temp;
    const auto path = temp.path / "title" / u8"précompiled.nx2prep";
    const std::vector<u8> bytes((1 << 20) + 17, 0xa5);
    REQUIRE(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Written);
    const auto earlier = std::filesystem::file_time_type::clock::now() - std::chrono::hours{24};
    std::filesystem::last_write_time(path, earlier);
    const auto prepared_timestamp = std::filesystem::last_write_time(path);

    REQUIRE(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Unchanged);
    CHECK(std::filesystem::last_write_time(path) == prepared_timestamp);
    CHECK(Read(path) == bytes);
    auto temporary = path;
    temporary += ".tmp";
    CHECK_FALSE(std::filesystem::exists(temporary));
}

TEST_CASE("Changed recipe contents replace the prepared file even at the same size",
          "[video_core][precompiled_file]") {
    TempDir temp;
    const auto path = temp.path / "precompiled.nx2prep";
    std::vector<u8> bytes((1 << 20) + 17, 0x42);
    REQUIRE(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Written);
    std::filesystem::last_write_time(
        path, std::filesystem::file_time_type::clock::now() - std::chrono::hours{24});
    const auto previous_timestamp = std::filesystem::last_write_time(path);
    bytes.back() ^= 1;
    REQUIRE(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Written);
    CHECK(Read(path) == bytes);
    CHECK(std::filesystem::last_write_time(path) != previous_timestamp);

    bytes.pop_back();
    REQUIRE(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Written);
    CHECK(Read(path) == bytes);
}

TEST_CASE("Failed recipe writes preserve the previous prepared file",
          "[video_core][precompiled_file]") {
    TempDir temp;
    const auto path = temp.path / "precompiled.nx2prep";
    const std::vector<u8> original{1, 2, 3, 4};
    REQUIRE(SavePrecompiledFile(path, original) == SavePrecompiledResult::Written);
    auto blocked_temporary = path;
    blocked_temporary += ".tmp";
    std::filesystem::create_directory(blocked_temporary);
    const std::vector<u8> changed{4, 3, 2, 1};
    CHECK(SavePrecompiledFile(path, changed) == SavePrecompiledResult::Failed);
    CHECK(Read(path) == original);
    CHECK(SavePrecompiledFile(path, {}) == SavePrecompiledResult::Failed);
    CHECK(Read(path) == original);
}

TEST_CASE("Failed prepared-file replacement leaves the destination intact",
          "[video_core][precompiled_file]") {
    TempDir temp;
    const auto path = temp.path / "precompiled.nx2prep";
    std::filesystem::create_directory(path);
    std::ofstream(path / "keep.txt") << "keep";
    const std::vector<u8> bytes{1, 2, 3};
    CHECK(SavePrecompiledFile(path, bytes) == SavePrecompiledResult::Failed);
    CHECK(std::filesystem::is_directory(path));
    CHECK(std::filesystem::exists(path / "keep.txt"));
    auto temporary = path;
    temporary += ".tmp";
    CHECK_FALSE(std::filesystem::exists(temporary));
}

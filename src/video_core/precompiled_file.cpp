// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

#include "video_core/precompiled_file.h"

namespace VideoCommon {
namespace {

// Adapted from Frizti/Fritchii's SameBytes (3d216bc066).
bool SameBytes(const std::filesystem::path& path, std::span<const u8> bytes) {
    std::error_code error;
    if (std::filesystem::file_size(path, error) != bytes.size() || error) {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    std::vector<char> chunk(size_t{1} << 20);
    size_t at = 0;
    while (at < bytes.size()) {
        const size_t wanted = std::min(chunk.size(), bytes.size() - at);
        if (!file.read(chunk.data(), static_cast<std::streamsize>(wanted)) ||
            std::memcmp(chunk.data(), bytes.data() + at, wanted) != 0) {
            return false;
        }
        at += wanted;
    }
    return true;
}

} // namespace

SavePrecompiledResult SavePrecompiledFile(const std::filesystem::path& path,
                                         std::span<const u8> bytes) {
    if (bytes.empty()) {
        return SavePrecompiledResult::Failed;
    }
    if (SameBytes(path, bytes)) {
        return SavePrecompiledResult::Unchanged;
    }
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return SavePrecompiledResult::Failed;
    }
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) {
        return SavePrecompiledResult::Failed;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) {
        std::filesystem::remove(temporary, error);
        return SavePrecompiledResult::Failed;
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return SavePrecompiledResult::Failed;
    }
    return SavePrecompiledResult::Written;
}

} // namespace VideoCommon

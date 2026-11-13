// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <span>

#include "common/common_types.h"

namespace VideoCommon {

enum class SavePrecompiledResult { Unchanged, Written, Failed };

/// Keep an identical file and its timestamp: preparation stamps depend on that timestamp.
/// Otherwise replace it through a temporary file, preserving the old file on write failure.
[[nodiscard]] SavePrecompiledResult SavePrecompiledFile(const std::filesystem::path& path,
                                                       std::span<const u8> bytes);

} // namespace VideoCommon

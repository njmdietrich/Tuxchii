#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 Lichiistein contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Builds the fsr4-vk FSR 4 library for Linux x86_64 from its pinned commit and AMD's SDK download,
# then collects what has to ship with it. Usage: build-fsr4-vk.sh <output folder>
#
# Needs git, curl, unzip, cmake, a C++20 compiler, llvm-objdump, and Python 3.10+ (with the
# zstandard package before 3.14). About 1.5 GB of temporary space. FSR4_AMD_ZIP can name a copy of
# AMD's zip that is already on disk; FSR4_WORK_DIR keeps the work folder instead of deleting it.

set -euo pipefail

readonly FSR4_VK_REPO=https://github.com/Astyyyyy/fsr4-vk.git
readonly FSR4_VK_COMMIT=b0125ea9798633750f83ebabbc00279bcb49d8dd
readonly AMD_ZIP_URL=https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/download/v2.3.0/FidelityFX-Samples-v2.3.0-prebuilt.zip
readonly AMD_ZIP_SHA256=f90890b9323bb2f4f2404ac4cdc9395e8495ecdac6f7aa0bcdf1ad1848422273
readonly AMD_DLL=Samples/Upscalers/FidelityFX_FSR/dx12/x64/Release/amd_fidelityfx_upscaler_dx12.dll

[[ $# -eq 1 ]] || { echo "usage: $0 <output folder>" >&2; exit 2; }
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$1"
output="$(cd "$1" && pwd)"

if [[ -n "${FSR4_WORK_DIR:-}" ]]; then
    work="$FSR4_WORK_DIR"
    mkdir -p "$work"
else
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
fi

zip="${FSR4_AMD_ZIP:-}"
if [[ -z "$zip" ]]; then
    zip="$work/amd-sdk.zip"
    curl -fsSL --retry 3 -o "$zip" "$AMD_ZIP_URL"
fi
echo "$AMD_ZIP_SHA256  $zip" | sha256sum --check --quiet
unzip -qjo "$zip" "$AMD_DLL" -d "$work/dll"

source="$work/fsr4-vk"
if [[ ! -d "$source/.git" ]]; then
    git init -q "$source"
    git -C "$source" remote add origin "$FSR4_VK_REPO"
    git -C "$source" fetch -q --depth 1 origin "$FSR4_VK_COMMIT"
    git -C "$source" checkout -q FETCH_HEAD
    git -C "$source" submodule update --init --recursive
fi
[[ "$(git -C "$source" rev-parse HEAD)" == "$FSR4_VK_COMMIT" ]]

# The GPL's corresponding source, taken before anything is generated in the tree.
tar -czf "$output/fsr4-vk-${FSR4_VK_COMMIT:0:7}-source.tar.gz" \
    --exclude-vcs --exclude='./build' --exclude='./assets' \
    --transform "s,^\.,fsr4-vk-${FSR4_VK_COMMIT:0:7}," -C "$source" .

python3 "$source/tools/setup.py" --dll "$work/dll/amd_fidelityfx_upscaler_dx12.dll"
FSR4_BUILD_JOBS="$(nproc)" bash "$source/tools/build/linux.sh"

library="$source/build/linux/amd_fidelityfx_upscaler_vk.so"
nm -D --defined-only "$library" | grep -q ' ffxCreateContext$'
install -m 755 "$library" "$output/"
strip --strip-unneeded "$output/amd_fidelityfx_upscaler_vk.so"

licenses="$output/fsr4-vk-licenses"
rm -rf "$licenses"
mkdir -p "$licenses"
cp -r "$source/LICENSE" "$source/LICENSES" "$source/THIRD_PARTY_NOTICES.md" "$licenses/"
cp "$repo_root/externals/fsr4-vk/NOTICE.md" "$licenses/"

ls -la "$output"

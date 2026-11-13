<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-License-Identifier: GPL-2.0-or-later
-->

## nx2 + Lichiistein Linux PGO integration

This branch combines upstream Lichii `nx2` at `f1cd98c105` with Lichiistein at `8f55ee5593`,
preserving both histories. Some Lichiistein changes already have newer upstream equivalents;
those upstream implementations are retained. See [the integration notes](docs/nx2/lichiistein-integration.md).

The `linux-x86_64-pgo` workflow builds only that platform variant, including the Qt frontend,
CLI, and the fsr4-vk runtime with embedded shaders and models. Its AppImage and self-contained
tar.gz are workflow artifacts; automatic update checks are disabled for these builds.

The Lichiistein overview below describes the imported fork; its original base and testing
reports are historical. Current upstream also supports FSR 4 on Linux.

## Actually, Lichiistein was the name of the scientist.

**Lichiistein** is an unofficial, personal fork of [Lichii](https://github.com/zortos293/Lichii), tuned for Ocarina of Time on AMD GPUs under Linux (RADV). 

It is based on Lichii `nx2` at `093e264c8a` and adds work that hasn't landed upstream yet. Credit belongs to the original authors, whose commits are preserved in the history.

**From open PRs and other forks**

- GPU-thread and fast-draw work: #836, #806, #802, #784, #865, #875, #785
- The mesh shader setting and fallback changes (#814)
- The fix for flashes on Low/Default fences (#874)
- On-disk shader precompilation (#895/#896)
- #995, #976, #974, #954, #970, #959, #1076, #933, #1051, #1008, #1023 and #913
- Eglantine-Community #673
- Fritchii's ASTC bounds fix for AMD corruption and device loss

**Added or adapted here**

- A RADV patch that leaves pipeline caching to Mesa.
- A rescue for Ocarina of Time's stuck flip counter, narrowed from Eglantine #712. It fixes a permanent hang seen when closing the quick menu in Hyrule Field. `EDEN_NX2_OOT_SEM_RESCUE=0` turns it off.
- An opt-in guard against the NaN "stretched glowing geometry" bug, from PantACRO4life's Lichii branch: `EDEN_NX2_NAN_GUARD=all`. It's a workaround and hasn't been proven in long play.
- A Skip button on the shader preparation screen.
- Update checks that follow this fork's releases, not upstream Lichii's.

**FSR 4**

This integration retains upstream's current FSR 4 support and Lichiistein's Linux runtime override. Turn on **Use AMD FSR 4 in place of FSR 3.1** in the Graphics settings, with Ocarina of Time's FSR setting also on.

- **Linux (RADV):** x86_64 release builds include `amd_fidelityfx_upscaler_vk.so`, built from [fsr4-vk](https://github.com/Astyyyyy/fsr4-vk) and AMD's FSR 4.1.1 shaders and weights. To build it yourself, run `tools/fsr4/build-fsr4-vk.sh`. It's an unofficial build, not made or endorsed by AMD, and its licences are listed in [the notice](externals/fsr4-vk/NOTICE.md).
- **Windows (AMD's driver):** the setting's panel downloads AMD's signed FidelityFX runtime (FSR 4.1.2) from AMD after showing AMD's licence. Nothing of AMD's is in the Windows package.

AMD, FidelityFX and FSR are trademarks of Advanced Micro Devices, Inc.

**Status**

This has only been played on an RX 9070 XT with Mesa 26.2.6, and the Windows route hasn't been tested at all, so expect rough edges. Please don't report problems from this build to upstream Lichii.

No game data or precompile recipes are included. Shader preparation only runs if you've made a recipe from your own game with `tools/nx2-precompile`.

<h2 align="center">⚠️ Lichii has no official website ⚠️</h2>

> [!CAUTION]
> **Lichii has no official website.** The only site run by this project is the **game compatibility website, [lichii.zortos.me](https://lichii.zortos.me)**, for compatibility reports only. Download Lichii **only** from this repository's [releases page](https://github.com/zortos293/Lichii/releases). Any other "Lichii" website or download (such as lichii.org) is not ours and may contain malware ([#545](https://github.com/zortos293/Lichii/issues/545)).

<p align="center">
  <img src="dist/branding/lichii_icon_with_logo.png" alt="Lichii" width="600">
</p>

# Lichii

Lichii is an experimental, open-source Nintendo Switch and Switch 2 emulator written in C++. It builds on Eglantine, Eden, yuzu and Sudachi, preserving their contributions while developing this fork's game compatibility and renderer fixes.

This is an independent fork, not an official release of those upstream projects. Lichii is not affiliated with Nintendo.

Visit the [Lichii game compatibility website](https://lichii.zortos.me) to find games and compare community compatibility reports.

## Downloads and setup

Download Lichii only from this repository's [releases page](https://github.com/zortos293/Lichii/releases). Lichii has no other download site: archives offered elsewhere, such as lichii.org, are not built by this project, and one has been reported to install malware ([#545](https://github.com/zortos293/Lichii/issues/545)).

On Windows, extract the entire build archive and run **lichii.exe**. Only use games and system files you have legally obtained. Games, keys and firmware are not included.

### Existing saves and settings

Lichii uses its own profile: `%APPDATA%\lichii` on Windows, and the `lichii` directories under `$XDG_DATA_HOME`, `$XDG_CONFIG_HOME` and `$XDG_CACHE_HOME` on Linux and macOS. Without XDG overrides, these are `~/.local/share/lichii`, `~/.config/lichii` and `~/.cache/lichii`. An existing portable `user/` directory still takes priority and is not imported or changed by the wizard.

On the first non-portable GUI launch, Lichii offers to import Eden and other detected emulator profiles. Back up `%APPDATA%\eden` first on Windows, or the old data, config and cache directories on Unix, and close the source emulator. Choose **Keep Old Data** to copy without changing the source, then check your saves and relaunch. Existing destination data is retained in backup directories rather than merged. Move also retains a source backup until you remove it yourself; Link deliberately shares settings and saves. Paths outside the imported data root remain shared and unchanged. An interrupted import offers recovery on the next launch. `eden-cli`, including `--help` and `--version`, refuses to initialize configuration until you import or explicitly choose **Start fresh** in the GUI.

## Compatibility and issue reports

Compatibility and performance vary by game, hardware and driver. Rendering errors, stutters and crashes remain possible. Passing automated tests does not establish that a game renders correctly, and this project makes no universal frame-rate guarantee.

Browse [community compatibility reports](https://lichii.zortos.me/reports) for results with each player's build and setup.

Desktop reporting requires a build containing [PR #303](https://github.com/zortos293/Lichii/pull/303) and is not available in build 88. In builds that include it, play and stop a Switch 2 game, then right-click that game and choose **Submit compatibility report**. Only the latest session in the current Lichii process is eligible.

Confirm the matching website catalog entry and review the editable machine, game, setup, and enabled add-on details prefilled where available. Check the public preview and give explicit consent before submitting. Nothing uploads automatically, and the native v1 reporter does not read or upload logs or files.

When [reporting an issue](https://github.com/zortos293/Eglantine/issues), include:

- The exact build, game version, CPU, GPU and driver version.
- Relevant settings and steps to reproduce the problem.
- A screenshot and the log from the affected session, after checking it for personal information.

Do not upload game files, keys, firmware or private account information.

## Building and contributing

See the [build guide](docs/Build.md), [user documentation](docs/user/README.md) and [coding guidelines](docs/Coding.md). Some inherited documentation and internal build target names still use upstream names. The GUI build target is `lichii`; its output is `lichii.exe` on Windows and `lichii` on other desktop platforms.

Keep changes focused and distinguish code-level checks from actual gameplay testing.

## Artwork
Art created by YOB (yobslab)
The supplied Lichii logo, icon and library artwork are in [dist/branding](dist/branding). Windows, Linux and macOS icon resources are generated from that artwork by [Generate-LichiiIcons.ps1](tools/Generate-LichiiIcons.ps1).

## Support Lichii

Lichii is free and stays free. If you want to help, you can donate in crypto. Donations help pay for the AI tools and build machines used to develop this open-source project faster.

**Solana (SOL)**
```text
JepEjfyWjGjhJvHSrqj52KtLjcbbbmUh1iGqerYBF4w
```

**Ethereum (ETH)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Bitcoin (BTC)**
```text
bc1qcvxdpf4qvdhqp8ee4s355cjmxrzsphsdwpl8w4
```

**BNB Smart Chain (BNB)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Polygon**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

Send each coin on its matching network. Coins sent on the wrong network can't be recovered.

> [!NOTE]
> Donations are voluntary gifts and are non-refundable. A donation does not buy any product, service, perk, early access, exclusive build, priority support or say in the project. Every release stays free and public for everyone.

## Credits and license

Lichii inherits the work of the Eglantine, Eden, yuzu and Sudachi projects and their contributors, including Eglantine's original developer Camille LaVey. Original copyright notices and third-party licenses are retained.

See [LICENSE.txt](LICENSE.txt) and the source files for applicable license terms.

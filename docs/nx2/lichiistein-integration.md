# Lichii nx2 + Lichiistein integration

The integration starts from original upstream Lichii, rather than making Lichiistein's older
base the main line. This keeps future `nx2` merges straightforward. A merge preserves the
original authors and commits from both repositories, including changes already incorporated
upstream by other commits.

Sources at the start of the integration:

- `zortos293/Lichii:nx2`: `21d62e43caa178aeb04e74585b3aad622b278b84`.
- `auz466/Lichiistein:lichiistein`: `8f55ee5593f14b4dbc94f5393eb1692ca200778f`.
- Git reports 96 non-merge Lichiistein commits without patch-equivalent commits in upstream.

## Merge decisions

- Keep Lichiistein's recipes, precompilation, driver-crash journal, canonical native pipeline
  keys, AMD mesh fallback, image recycling, draw and buffer changes, and Ocarina patches.
- Adapt precompiled graphics pipelines to upstream's SPIR-V retention API, so eviction can
  rebuild them without translating again. Preserve upstream's geometry-output pruning,
  descriptor limits, pipeline budgets, and unmapped-pointer checks in the shared translator.
- Keep upstream's newer SSA rewrite and add Lichiistein's translation timing instrumentation.
- Combine Lichiistein's synchronized payload-fence handling with upstream's compact report
  callback, and keep both sets of fast-draw methods.
- Keep upstream's Linux FSR 4 device-feature checks and INT8/FP8 model selection. Retain
  `EDEN_NX2_FSR4_LIB` and user-installed runtime overrides from Lichiistein.

## Linux x86-64 PGO workflow

`.github/workflows/release-binaries.yml` has a single Ubuntu 24.04 x86-64 job. It runs on
pushes affecting build inputs on `nx2-lichiistein-linux-pgo`, or through `workflow_dispatch`
on a chosen branch. It uploads artifacts without creating tags or releases.

- Use clang/lld 23 and the checked-in `eglantine.profdata`, verified with llvm-profdata.
- Build CMake targets `yuzu` (the `lichii` Qt binary) and `yuzu-cmd` (`eden-cli`).
- Run `tools/fsr4-vk/build.sh` to verify AMD's pinned DLL, generate shader/model assets,
  and build `amd_fidelityfx_upscaler_vk` with embedded, compressed assets. The shader conversion
  tools are built automatically from fsr4-vk's pinned submodules during setup.
- Package only `linux-x86_64-pgo`, as an AppImage and a self-contained tar.gz. Both must contain
  `usr/lib/amd_fidelityfx_upscaler_vk.so` and the runtime and AMD licenses; the workflow checks
  their contents and the runtime's FFX exports.
- Disable automatic update checks because these artifacts do not correspond to upstream releases.

On supported Radeon hardware with RADV, select Ocarina's FSR upscaler and enable the FSR 4
replacement in Graphics settings. FSR 3.1 remains the fallback when runtime/device requirements
are unmet. FP8 additionally needs the cooperative-matrix features described in
[native-dlss-replacement.md](../native-dlss-replacement.md); packaging cannot enable missing
GPU or driver capabilities.

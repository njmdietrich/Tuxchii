# Shader eviction backport

`shader-eviction.patch` ports these Lichii changes by Zortos and capy-ai[bot]:

- [#1334 / e63d0a04d](https://github.com/zortos293/Lichii/commit/e63d0a04dfa93a40c185fa16a1be69a134121ea9): keep graphics pipelines within a RAM budget and optionally build older cached shaders before starting a game.
- [#1337 / be2414796](https://github.com/zortos293/Lichii/commit/be24147966d78ab680bd873242289e3c70570e3e): rebuild evicted pipelines from compressed SPIR-V instead of translating their guest shaders again.

Desktop eviction starts above the number of built pipelines estimated to fit in a quarter of RAM, at 300 KiB per pipeline. Only pipelines unused for 9,000 frames qualify, and the oldest are evicted first. Retained SPIR-V and metadata use a separate store bounded to 1/64 of RAM; its oldest entries are discarded first. GPU work finishes before evicted driver objects are destroyed.

The port keeps Fritchii's pipeline libraries, shader hashes, mesh emulation, background builds, precompiled shader preparation, asynchronous translation and existing storage-pointer preservation from Lichii #1143. It preserves `EDEN_NX2_PIPELINE_EVICT_ABOVE` and `EDEN_NX2_PRELOAD_ALL_PIPELINES`. The latter also has a per-game **Graphics > Advanced > Build all cached shaders before game start** setting, off by default. Recent sessions retain priority over extra boot builds. Android retains frame-based eviction; MoltenVK retains its existing exemption.

The disk shader-cache format is unchanged.

## Build integration

This branch is based on Tuxchii `testing` at `b2d5c4d0eb55df48a80957f046c85a377f9297a9`. Its AppImage workflow checks out a Fritchii release and applies `.github/linux-patches/*.patch` in filename order. The backport therefore lives here, rather than in Tuxchii's older source snapshot, so test AppImages include it automatically.

To build it, run **appimage-auto-release** from `patch/lichii-shader-eviction` with tag `fritchii-v5.8.4` (or `fritchii-v5.8`). A run from this branch uploads a test AppImage artifact.

## Validation

- Applied the workflow's complete patch step, including branding and all existing Linux patches, successfully to clean `fritchii-v5.8` and `fritchii-v5.8.4` checkouts.
- Built and ran the imported eviction-policy and bounded-store tests using Clang 19 and Ninja: 9 test cases, 10,929 assertions passed.
- Compiled `vk_pipeline_cache.cpp`, `vk_graphics_pipeline.cpp` and `shared_translation.cpp` from the patched v5.8.4 source with Clang 19 and `-Werror`. Used the release's Vulkan Headers v1.4.345 and VMA v3.3.0.
- A complete AppImage build and gameplay test have not been run.

The patch registers the tests in the project's normal CMake test target. With a configured project build, run:

```sh
cmake --build build --target tests --parallel
build/bin/tests '[pipeline_eviction],[bounded_store]'
```

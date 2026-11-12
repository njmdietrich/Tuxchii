# FSR 4 library: notices

`amd_fidelityfx_upscaler_vk.so` is built from [fsr4-vk](https://github.com/Astyyyyy/fsr4-vk), commit
`b0125ea9798633750f83ebabbc00279bcb49d8dd`, by the fsr4-vk contributors.

- **fsr4-vk's own code** is licensed under GPL-3.0-only (`LICENSE`). Its complete source for this
  build, `fsr4-vk-b0125ea-source.tar.gz`, is attached to each release. fsr4-vk's
  `THIRD_PARTY_NOTICES.md` and `LICENSES/` folder cover the components it includes.
- **The FSR 4.1.1 shaders and model weights** embedded in the library come from
  `amd_fidelityfx_upscaler_dx12.dll` in AMD's FidelityFX SDK 2.3.0, which AMD's SDK licence lists
  among its MIT-licensed files. See `LICENSES/AMD-FidelityFX-SDK-MIT.md` and
  `LICENSES/AMD-FidelityFX-SDK-license.md`. Copyright (c) Advanced Micro Devices, Inc.
- `tools/fsr4/build-fsr4-vk.sh` in this repository rebuilds the library from those sources.

This is an unofficial build. It is not made, endorsed or supported by AMD. AMD, FidelityFX and FSR
are trademarks of Advanced Micro Devices, Inc. Please don't report problems with it to AMD.

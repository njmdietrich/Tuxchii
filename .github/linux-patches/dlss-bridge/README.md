# Fritchii DLSS bridge (Linux)

NVIDIA DLSS for the Fritchii Linux AppImages, without shipping anything of NVIDIA's.

On Linux, DLSS is NVIDIA's NGX: its core (`libnvidia-ngx.so.1`) comes with the NVIDIA driver, but
applications reach it through `libnvsdk_ngx.a` and use the DLSS model `libnvidia-ngx-dlss.so.*`,
both from NVIDIA's [DLSS SDK](https://github.com/NVIDIA/DLSS) under NVIDIA's license. That license
doesn't allow the SDK to become part of an open source (GPL) program, so:

- **Fritchii (GPL)** only loads `libfritchii_dlss_bridge.so` at run time (`dlopen`), from
  `~/.local/share/eden/nvidia-dlss/`. It contains no NVIDIA code.
- **This bridge (MIT)** is the only code that calls NGX. It declares the few NGX functions it uses
  itself and is compiled on its own; the AppImage carries it as an object file
  (`usr/share/fritchii-dlss/fritchii_dlss_bridge.o`) with the [mold](https://github.com/rui314/mold)
  linker (MIT).
- **Settings > Graphics > Install DLSS** shows NVIDIA's license; once the user accepts it, it
  downloads `libnvsdk_ngx.a` and the DLSS model from NVIDIA's repository (pinned release, SHA-256
  checked) and links the bridge **on the user's PC**. Nothing of NVIDIA's passes through this
  repository or its releases.

Files: `fritchii_dlss_bridge.h` (the C interface, also in the Fritchii patch),
`fritchii_dlss_bridge.cpp` (the NGX calls), `fritchii_dlss_dso.c` (what `crtbeginS.o` would add,
since the user's PC links with a plain linker).

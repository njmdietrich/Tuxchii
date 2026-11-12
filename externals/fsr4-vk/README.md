# fsr4-vk

Linux x86_64 releases ship `amd_fidelityfx_upscaler_vk.so`, the FSR 4 library that the "Use AMD FSR 4
in place of FSR 3.1" setting loads on RADV. No binary is kept in this repository:
`tools/fsr4/build-fsr4-vk.sh` builds it from [fsr4-vk](https://github.com/Astyyyyy/fsr4-vk) at a
pinned commit, and takes AMD's shaders and model weights from AMD's FidelityFX SDK 2.3.0 samples
download, checked against a pinned SHA-256.

To build it yourself, for a local build or a distribution package:

```sh
tools/fsr4/build-fsr4-vk.sh fsr4-output
cp fsr4-output/amd_fidelityfx_upscaler_vk.so ~/.local/share/eden/
```

Next to the program works as well. The script lists the tools it needs. It also writes the
fsr4-vk source archive and the licence folder that must go with any copy you distribute.

Windows doesn't use this library: there the setting's panel downloads AMD's own signed FidelityFX
DLLs from AMD's SDK repository.

See [NOTICE.md](NOTICE.md) for attribution and licences.

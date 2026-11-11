// SPDX-FileCopyrightText: Copyright 2026 Fritchii-linux contributors
// SPDX-License-Identifier: MIT

// What a compiler driver's crtbeginS.o gives a shared library. The bridge is linked on the user's
// PC with a plain linker and no compiler, so it brings its own: the DSO handle C++ static
// destructors register with, and running those destructors when the library is unloaded.

extern void __cxa_finalize(void* dso) __attribute__((weak));

__attribute__((visibility("hidden"))) void* __dso_handle = &__dso_handle;

__attribute__((destructor)) static void fdb_finalize(void) {
    if (__cxa_finalize) {
        __cxa_finalize(__dso_handle);
    }
}

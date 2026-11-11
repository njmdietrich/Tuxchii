// SPDX-FileCopyrightText: Copyright 2026 Fritchii-linux contributors
// SPDX-License-Identifier: MIT

// The Fritchii DLSS bridge: a small C interface between Fritchii (GPL) and NVIDIA's DLSS SDK.
//
// Fritchii only loads the bridge at run time (dlopen) and never contains NVIDIA code. The bridge
// itself is linked on the user's own PC, from this MIT-licensed code and the NVIDIA DLSS SDK
// files the user downloads from NVIDIA (Settings > Graphics > Install DLSS), under NVIDIA's license.

#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FDB_ABI_VERSION 2u

typedef struct FdbImage {
    VkImageView view;
    VkImage image;
    VkImageSubresourceRange range;
    VkFormat format;
    uint32_t width;
    uint32_t height;
} FdbImage;

typedef struct FdbEvaluate {
    FdbImage color;  ///< Jittered colour at the render size (sampled-image state)
    FdbImage depth;  ///< Depth at the render size (sampled-image state)
    FdbImage motion; ///< Motion vectors at the render size (sampled-image state)
    FdbImage output; ///< Output at the upscale size (storage-image state)
    uint32_t render_width;
    uint32_t render_height;
    uint32_t output_width;
    uint32_t output_height;
    int32_t quality;       ///< 0 performance, 1 balanced, 2 quality, 3 ultra performance, 5 DLAA
    uint32_t preset;       ///< 0 default, 10..13 = presets J..M
    uint32_t create_flags; ///< 1 HDR, 2 low-res motion vectors, 4 jittered MVs, 8 inverted depth,
                           ///< 64 auto exposure
    float jitter_x;        ///< Sample offset in render pixels
    float jitter_y;
    float mv_scale_x; ///< Turns a motion vector into render pixels
    float mv_scale_y;
    int32_t reset;
} FdbEvaluate;

/// level: 0 info, 1 warning, 2 error
typedef void (*FdbLogFn)(int level, const char* message);

/// FDB_ABI_VERSION of the bridge.
uint32_t fdb_abi_version(void);

/// The Vulkan instance and device extensions DLSS needs. The names stay valid while the bridge is
/// loaded. Returns 1 on success.
int fdb_required_extensions(uint32_t* instance_count, const char* const** instance_names,
                            uint32_t* device_count, const char* const** device_names);

/// Starts NGX on the renderer's device and checks that DLSS is available. data_dir is a writable
/// folder for NGX's own files; model_dir holds libnvidia-ngx-dlss.so.*. Returns 1 when DLSS can
/// run, otherwise 0 (fdb_last_error says why).
int fdb_init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
             PFN_vkGetInstanceProcAddr get_instance_proc_addr,
             PFN_vkGetDeviceProcAddr get_device_proc_addr, const char* data_dir_utf8,
             const char* model_dir_utf8, FdbLogFn log);

/// The render sizes DLSS accepts for an output size in a quality mode (see FdbEvaluate::quality).
/// Returns 1 on success.
int fdb_render_size_range(uint32_t output_width, uint32_t output_height, int32_t quality,
                          uint32_t* min_width, uint32_t* min_height, uint32_t* max_width,
                          uint32_t* max_height);

/// Records one DLSS evaluation (creating or recreating the DLSS feature when its settings change).
/// Returns 1 on success.
int fdb_evaluate(VkCommandBuffer command_buffer, const FdbEvaluate* evaluate);

/// Releases DLSS and shuts NGX down. The device must be idle.
void fdb_shutdown(void);

/// The last failure, in English.
const char* fdb_last_error(void);

#ifdef __cplusplus
}
#endif

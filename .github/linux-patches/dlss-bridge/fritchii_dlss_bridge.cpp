// SPDX-FileCopyrightText: Copyright 2026 Fritchii-linux contributors
// SPDX-License-Identifier: MIT

// The Fritchii DLSS bridge (see fritchii_dlss_bridge.h).
//
// This file holds no NVIDIA code: it declares the few NGX functions and structures it calls, as
// documented in NVIDIA's DLSS Programming Guide, and is compiled on its own. The user's PC links it
// with libnvsdk_ngx.a from the NVIDIA DLSS SDK they downloaded, into libfritchii_dlss_bridge.so.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "fritchii_dlss_bridge.h"

#define FDB_EXPORT __attribute__((visibility("default")))

namespace {

// --- NGX declarations (C linkage, as exported by libnvsdk_ngx.a) ---------------------------------

using NgxResult = uint32_t;
constexpr NgxResult NGX_SUCCESS = 0x1;
inline bool NgxFailed(NgxResult result) {
    return (result & 0xFFF00000u) == 0xBAD00000u;
}

constexpr int NGX_FEATURE_SUPER_SAMPLING = 1;
constexpr int NGX_ENGINE_TYPE_CUSTOM = 0;
constexpr int NGX_VERSION_API = 0x15;
constexpr int NGX_LOGGING_LEVEL_ON = 1;
constexpr int NGX_RESOURCE_VK_TYPE_IMAGEVIEW = 0;

struct NgxParameter;
struct NgxHandle;

using NgxLogCallback = void (*)(const char* message, int level, int source_component);

struct NgxPathListInfo {
    const wchar_t* const* path;
    unsigned int length;
};

struct NgxLoggingInfo {
    NgxLogCallback callback;
    int minimum_level;
    bool disable_other_sinks;
};

struct NgxFeatureCommonInfo {
    NgxPathListInfo path_list;
    void* internal_data;
    NgxLoggingInfo logging;
};

struct NgxImageViewInfoVk {
    VkImageView image_view;
    VkImage image;
    VkImageSubresourceRange subresource_range;
    VkFormat format;
    unsigned int width;
    unsigned int height;
};

struct NgxBufferInfoVk {
    VkBuffer buffer;
    unsigned int size_in_bytes;
};

struct NgxResourceVk {
    union {
        NgxImageViewInfoVk image_view;
        NgxBufferInfoVk buffer;
    } resource;
    int type;
    bool read_write;
};

extern "C" {
NgxResult NVSDK_NGX_VULKAN_RequiredExtensions(unsigned int* instance_count,
                                              const char*** instance_names,
                                              unsigned int* device_count,
                                              const char*** device_names);
NgxResult NVSDK_NGX_VULKAN_Init_with_ProjectID(
    const char* project_id, int engine_type, const char* engine_version,
    const wchar_t* application_data_path, VkInstance instance, VkPhysicalDevice physical_device,
    VkDevice device, PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
    const NgxFeatureCommonInfo* feature_info, int sdk_version);
NgxResult NVSDK_NGX_VULKAN_Shutdown1(VkDevice device);
NgxResult NVSDK_NGX_VULKAN_GetCapabilityParameters(NgxParameter** parameters);
NgxResult NVSDK_NGX_VULKAN_AllocateParameters(NgxParameter** parameters);
NgxResult NVSDK_NGX_VULKAN_DestroyParameters(NgxParameter* parameters);
NgxResult NVSDK_NGX_VULKAN_CreateFeature1(VkDevice device, VkCommandBuffer command_buffer,
                                          int feature, NgxParameter* parameters,
                                          NgxHandle** handle);
NgxResult NVSDK_NGX_VULKAN_ReleaseFeature(NgxHandle* handle);
NgxResult NVSDK_NGX_VULKAN_EvaluateFeature_C(VkCommandBuffer command_buffer,
                                             const NgxHandle* handle,
                                             const NgxParameter* parameters,
                                             void (*progress)(float, bool*));
void NVSDK_NGX_Parameter_SetF(NgxParameter* parameters, const char* name, float value);
void NVSDK_NGX_Parameter_SetUI(NgxParameter* parameters, const char* name, unsigned int value);
void NVSDK_NGX_Parameter_SetI(NgxParameter* parameters, const char* name, int value);
void NVSDK_NGX_Parameter_SetVoidPointer(NgxParameter* parameters, const char* name, void* value);
NgxResult NVSDK_NGX_Parameter_GetI(NgxParameter* parameters, const char* name, int* value);
NgxResult NVSDK_NGX_Parameter_GetUI(NgxParameter* parameters, const char* name,
                                    unsigned int* value);
NgxResult NVSDK_NGX_Parameter_GetVoidPointer(NgxParameter* parameters, const char* name,
                                             void** value);
}

using NgxOptimalSettingsCallback = NgxResult (*)(NgxParameter* parameters);

// Fritchii's NGX project ID (the same one its Windows build gives NVIDIA Streamline).
constexpr const char* PROJECT_ID = "8d3e2f9a-6b41-4c7e-9a52-1f0c3b7d5e64";

// --- Bridge state -------------------------------------------------------------------------------

struct CreateSettings {
    uint32_t render_width{};
    uint32_t render_height{};
    uint32_t output_width{};
    uint32_t output_height{};
    int32_t quality{};
    uint32_t preset{};
    uint32_t create_flags{};

    bool operator==(const CreateSettings&) const = default;
};

struct State {
    VkDevice device{};
    bool started{};
    NgxParameter* parameters{};
    NgxParameter* capabilities{}; ///< Kept for DLSS's optimal settings callback
    NgxHandle* handle{};
    CreateSettings created{};
    FdbLogFn log{};
    std::wstring data_dir;
    std::wstring model_dir;
    char error[512]{};
};

State& GetState() {
    static State state;
    return state;
}

void Log(int level, const char* format, ...) {
    State& state = GetState();
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (level >= 1) {
        std::snprintf(state.error, sizeof(state.error), "%s", message);
    }
    if (state.log) {
        state.log(level, message);
    }
}

void NgxLog(const char* message, int /*level*/, int /*component*/) {
    State& state = GetState();
    if (state.log && message) {
        // NGX lines end with a newline.
        std::string line{message};
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        state.log(0, ("NGX: " + line).c_str());
    }
}

// UTF-8 to UTF-32 wchar_t (Linux).
std::wstring Widen(const char* text) {
    std::wstring out;
    const auto* s = reinterpret_cast<const unsigned char*>(text ? text : "");
    while (*s) {
        uint32_t code = *s;
        int extra = 0;
        if (code >= 0xF0) {
            code &= 0x07;
            extra = 3;
        } else if (code >= 0xE0) {
            code &= 0x0F;
            extra = 2;
        } else if (code >= 0xC0) {
            code &= 0x1F;
            extra = 1;
        }
        ++s;
        for (int i = 0; i < extra && (*s & 0xC0) == 0x80; ++i, ++s) {
            code = (code << 6) | (*s & 0x3F);
        }
        out.push_back(static_cast<wchar_t>(code));
    }
    return out;
}

NgxResourceVk Resource(const FdbImage& image, bool read_write) {
    NgxResourceVk resource{};
    resource.resource.image_view.image_view = image.view;
    resource.resource.image_view.image = image.image;
    resource.resource.image_view.subresource_range = image.range;
    resource.resource.image_view.format = image.format;
    resource.resource.image_view.width = image.width;
    resource.resource.image_view.height = image.height;
    resource.type = NGX_RESOURCE_VK_TYPE_IMAGEVIEW;
    resource.read_write = read_write;
    return resource;
}

void ReleaseFeature(State& state) {
    if (state.handle) {
        NVSDK_NGX_VULKAN_ReleaseFeature(state.handle);
        state.handle = nullptr;
    }
}

} // Anonymous namespace

extern "C" {

FDB_EXPORT uint32_t fdb_abi_version(void) {
    return FDB_ABI_VERSION;
}

FDB_EXPORT int fdb_required_extensions(uint32_t* instance_count, const char* const** instance_names,
                                       uint32_t* device_count, const char* const** device_names) {
    unsigned int instances = 0;
    unsigned int devices = 0;
    const char** instance_list = nullptr;
    const char** device_list = nullptr;
    const NgxResult result =
        NVSDK_NGX_VULKAN_RequiredExtensions(&instances, &instance_list, &devices, &device_list);
    if (NgxFailed(result)) {
        Log(2, "NGX could not list the Vulkan extensions DLSS needs (0x%08x)", result);
        return 0;
    }
    *instance_count = instances;
    *instance_names = instance_list;
    *device_count = devices;
    *device_names = device_list;
    return 1;
}

FDB_EXPORT int fdb_init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
                        PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                        PFN_vkGetDeviceProcAddr get_device_proc_addr, const char* data_dir_utf8,
                        const char* model_dir_utf8, FdbLogFn log) {
    State& state = GetState();
    if (state.started) {
        return 1;
    }
    state.log = log;
    state.data_dir = Widen(data_dir_utf8);
    state.model_dir = Widen(model_dir_utf8);

    const wchar_t* paths[] = {state.model_dir.c_str()};
    NgxFeatureCommonInfo info{};
    info.path_list.path = paths;
    info.path_list.length = 1;
    info.logging.callback = NgxLog;
    info.logging.minimum_level = NGX_LOGGING_LEVEL_ON;
    info.logging.disable_other_sinks = false;

    NgxResult result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        PROJECT_ID, NGX_ENGINE_TYPE_CUSTOM, "1.0", state.data_dir.c_str(), instance,
        physical_device, device, get_instance_proc_addr, get_device_proc_addr, &info,
        NGX_VERSION_API);
    if (NgxFailed(result)) {
        Log(2, "NGX did not start (0x%08x); is the NVIDIA driver installed?", result);
        return 0;
    }
    state.device = device;
    state.started = true;

    NgxParameter* capabilities = nullptr;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&capabilities);
    if (NgxFailed(result) || !capabilities) {
        Log(2, "NGX has no capability parameters (0x%08x)", result);
        fdb_shutdown();
        return 0;
    }
    int needs_driver = 0;
    unsigned int min_major = 0;
    unsigned int min_minor = 0;
    if (!NgxFailed(NVSDK_NGX_Parameter_GetI(capabilities, "SuperSampling.NeedsUpdatedDriver",
                                            &needs_driver)) &&
        needs_driver) {
        NVSDK_NGX_Parameter_GetUI(capabilities, "SuperSampling.MinDriverVersionMajor", &min_major);
        NVSDK_NGX_Parameter_GetUI(capabilities, "SuperSampling.MinDriverVersionMinor", &min_minor);
        Log(2, "DLSS needs NVIDIA driver %u.%u or newer", min_major, min_minor);
        NVSDK_NGX_VULKAN_DestroyParameters(capabilities);
        fdb_shutdown();
        return 0;
    }
    int available = 0;
    NVSDK_NGX_Parameter_GetI(capabilities, "SuperSampling.Available", &available);
    if (!available) {
        int init_result = 0;
        NVSDK_NGX_Parameter_GetI(capabilities, "SuperSampling.FeatureInitResult", &init_result);
        Log(2, "DLSS is not available on this GPU or driver (0x%08x)",
            static_cast<unsigned int>(init_result));
        NVSDK_NGX_VULKAN_DestroyParameters(capabilities);
        fdb_shutdown();
        return 0;
    }
    state.capabilities = capabilities;

    result = NVSDK_NGX_VULKAN_AllocateParameters(&state.parameters);
    if (NgxFailed(result) || !state.parameters) {
        Log(2, "NGX could not allocate parameters (0x%08x)", result);
        fdb_shutdown();
        return 0;
    }
    Log(0, "NVIDIA DLSS is available (NGX)");
    return 1;
}

FDB_EXPORT int fdb_render_size_range(uint32_t output_width, uint32_t output_height,
                                     int32_t quality, uint32_t* min_width, uint32_t* min_height,
                                     uint32_t* max_width, uint32_t* max_height) {
    State& state = GetState();
    if (!state.started || !state.capabilities) {
        Log(2, "DLSS is not started");
        return 0;
    }
    NgxParameter* const p = state.capabilities;
    void* callback = nullptr;
    NVSDK_NGX_Parameter_GetVoidPointer(p, "DLSSOptimalSettingsCallback", &callback);
    if (!callback) {
        Log(1, "This DLSS model has no optimal settings query");
        return 0;
    }
    NVSDK_NGX_Parameter_SetUI(p, "Width", output_width);
    NVSDK_NGX_Parameter_SetUI(p, "Height", output_height);
    NVSDK_NGX_Parameter_SetI(p, "PerfQualityValue", quality);
    NVSDK_NGX_Parameter_SetI(p, "RTXValue", 0);
    const NgxResult result = reinterpret_cast<NgxOptimalSettingsCallback>(callback)(p);
    if (NgxFailed(result)) {
        return 0;
    }
    unsigned int optimal_width = 0;
    unsigned int optimal_height = 0;
    NVSDK_NGX_Parameter_GetUI(p, "OutWidth", &optimal_width);
    NVSDK_NGX_Parameter_GetUI(p, "OutHeight", &optimal_height);
    if (optimal_width == 0 || optimal_height == 0) {
        return 0; // The mode isn't offered for this output size
    }
    // Older DLSS models report only the optimal size.
    unsigned int values[4] = {optimal_width, optimal_height, optimal_width, optimal_height};
    NVSDK_NGX_Parameter_GetUI(p, "DLSS.Get.Dynamic.Min.Render.Width", &values[0]);
    NVSDK_NGX_Parameter_GetUI(p, "DLSS.Get.Dynamic.Min.Render.Height", &values[1]);
    NVSDK_NGX_Parameter_GetUI(p, "DLSS.Get.Dynamic.Max.Render.Width", &values[2]);
    NVSDK_NGX_Parameter_GetUI(p, "DLSS.Get.Dynamic.Max.Render.Height", &values[3]);
    *min_width = values[0];
    *min_height = values[1];
    *max_width = values[2];
    *max_height = values[3];
    return 1;
}

FDB_EXPORT int fdb_evaluate(VkCommandBuffer command_buffer, const FdbEvaluate* e) {
    State& state = GetState();
    if (!state.started || !state.parameters || !e) {
        Log(2, "DLSS is not started");
        return 0;
    }
    NgxParameter* const p = state.parameters;
    const CreateSettings wanted{e->render_width, e->render_height, e->output_width,
                                e->output_height, e->quality,      e->preset,
                                e->create_flags};
    if (!state.handle || !(wanted == state.created)) {
        ReleaseFeature(state);
        NVSDK_NGX_Parameter_SetUI(p, "CreationNodeMask", 1);
        NVSDK_NGX_Parameter_SetUI(p, "VisibilityNodeMask", 1);
        NVSDK_NGX_Parameter_SetUI(p, "Width", e->render_width);
        NVSDK_NGX_Parameter_SetUI(p, "Height", e->render_height);
        NVSDK_NGX_Parameter_SetUI(p, "OutWidth", e->output_width);
        NVSDK_NGX_Parameter_SetUI(p, "OutHeight", e->output_height);
        NVSDK_NGX_Parameter_SetI(p, "PerfQualityValue", e->quality);
        NVSDK_NGX_Parameter_SetI(p, "DLSS.Feature.Create.Flags", static_cast<int>(e->create_flags));
        NVSDK_NGX_Parameter_SetI(p, "DLSS.Enable.Output.Subrects", 0);
        for (const char* name :
             {"DLSS.Hint.Render.Preset.DLAA", "DLSS.Hint.Render.Preset.Quality",
              "DLSS.Hint.Render.Preset.Balanced", "DLSS.Hint.Render.Preset.Performance",
              "DLSS.Hint.Render.Preset.UltraPerformance", "DLSS.Hint.Render.Preset.UltraQuality"}) {
            NVSDK_NGX_Parameter_SetUI(p, name, e->preset);
        }
        const NgxResult result = NVSDK_NGX_VULKAN_CreateFeature1(
            state.device, command_buffer, NGX_FEATURE_SUPER_SAMPLING, p, &state.handle);
        if (NgxFailed(result) || !state.handle) {
            state.handle = nullptr;
            Log(2, "DLSS could not be created for %ux%u -> %ux%u (0x%08x)", e->render_width,
                e->render_height, e->output_width, e->output_height, result);
            return 0;
        }
        state.created = wanted;
        Log(0, "NVIDIA DLSS created: %ux%u -> %ux%u, quality %d, preset %u, flags 0x%x",
            e->render_width, e->render_height, e->output_width, e->output_height, e->quality,
            e->preset, e->create_flags);
    }

    NgxResourceVk color = Resource(e->color, false);
    NgxResourceVk depth = Resource(e->depth, false);
    NgxResourceVk motion = Resource(e->motion, false);
    NgxResourceVk output = Resource(e->output, true);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Color", &color);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Output", &output);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Depth", &depth);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "MotionVectors", &motion);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "TransparencyMask", nullptr);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "ExposureTexture", nullptr);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "DLSS.Input.Bias.Current.Color.Mask", nullptr);
    NVSDK_NGX_Parameter_SetF(p, "Jitter.Offset.X", e->jitter_x);
    NVSDK_NGX_Parameter_SetF(p, "Jitter.Offset.Y", e->jitter_y);
    NVSDK_NGX_Parameter_SetF(p, "Sharpness", 0.0f);
    NVSDK_NGX_Parameter_SetI(p, "Reset", e->reset ? 1 : 0);
    NVSDK_NGX_Parameter_SetF(p, "MV.Scale.X", e->mv_scale_x == 0.0f ? 1.0f : e->mv_scale_x);
    NVSDK_NGX_Parameter_SetF(p, "MV.Scale.Y", e->mv_scale_y == 0.0f ? 1.0f : e->mv_scale_y);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.Color.Subrect.Base.X", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.Color.Subrect.Base.Y", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.Depth.Subrect.Base.X", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.Depth.Subrect.Base.Y", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.MV.Subrect.Base.X", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Input.MV.Subrect.Base.Y", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Output.Subrect.Base.X", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Output.Subrect.Base.Y", 0);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Render.Subrect.Dimensions.Width", e->render_width);
    NVSDK_NGX_Parameter_SetUI(p, "DLSS.Render.Subrect.Dimensions.Height", e->render_height);
    NVSDK_NGX_Parameter_SetF(p, "DLSS.Pre.Exposure", 1.0f);
    NVSDK_NGX_Parameter_SetF(p, "DLSS.Exposure.Scale", 1.0f);

    const NgxResult result =
        NVSDK_NGX_VULKAN_EvaluateFeature_C(command_buffer, state.handle, p, nullptr);
    // The resources above live on this stack only.
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Color", nullptr);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Output", nullptr);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "Depth", nullptr);
    NVSDK_NGX_Parameter_SetVoidPointer(p, "MotionVectors", nullptr);
    if (NgxFailed(result)) {
        Log(2, "DLSS evaluation failed (0x%08x)", result);
        return 0;
    }
    return 1;
}

FDB_EXPORT void fdb_shutdown(void) {
    State& state = GetState();
    if (!state.started) {
        return;
    }
    ReleaseFeature(state);
    if (state.parameters) {
        NVSDK_NGX_VULKAN_DestroyParameters(state.parameters);
        state.parameters = nullptr;
    }
    if (state.capabilities) {
        NVSDK_NGX_VULKAN_DestroyParameters(state.capabilities);
        state.capabilities = nullptr;
    }
    NVSDK_NGX_VULKAN_Shutdown1(state.device);
    state.started = false;
    state.created = {};
    Log(0, "NVIDIA DLSS (NGX) stopped");
}

FDB_EXPORT const char* fdb_last_error(void) {
    return GetState().error;
}

} // extern "C"

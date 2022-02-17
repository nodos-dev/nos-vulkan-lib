#pragma once

#ifndef VK_USE_PLATFORM_WIN32_KHR

#endif

#include <vkl.h>

#include <mzCommon.h>

void ReadInputLayout(const u32* src, u64 sz, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);

std::unordered_map<u32, std::vector<VkDescriptorSetLayoutBinding>> GetLayouts(const u32* src, u64 sz);

struct f32x2
{
    f32 x, y;
};
struct i32x2
{
    i32 x, y;
};
struct f32x4
{
    f32 x, y, z, w;
};

inline const char* vk_result_string(VkResult re)
{
    switch (re)
    {
    case VK_SUCCESS:
        return "SUCCESS";
    case VK_NOT_READY:
        return "NOT_READY";
    case VK_TIMEOUT:
        return "TIMEOUT";
    case VK_EVENT_SET:
        return "EVENT_SET";
    case VK_EVENT_RESET:
        return "EVENT_RESET";
    case VK_INCOMPLETE:
        return "INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
        return "ERROR_INVALID_SHADER_NV";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_EXT:
        return "ERROR_NOT_PERMITTED_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
        return "THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
        return "THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
        return "OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return "OPERATION_NOT_DEFERRED_KHR";
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return "PIPELINE_COMPILE_REQUIRED_EXT";
    case VK_RESULT_MAX_ENUM:
        return "RESULT_MAX_ENUM";
    }
    return "";
}

inline const char* descriptor_type_to_string(VkDescriptorType ty)
{
    switch (ty)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return "SAMPLER";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "COMBINED_IMAGE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "SAMPLED_IMAGE";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "STORAGE_IMAGE";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "UNIFORM_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "STORAGE_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "UNIFORM_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "STORAGE_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "UNIFORM_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "STORAGE_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "INPUT_ATTACHMENT";
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        return "INLINE_UNIFORM_BLOCK_EXT";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return "ACCELERATION_STRUCTURE_KHR";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
        return "ACCELERATION_STRUCTURE_NV";
    case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
        return "MUTABLE_VALVE";
    default:
        return "";
    }
}

#define CHECKRE(expr)                                                                       \
    {                                                                                       \
        VkResult re = (expr);                                                               \
        while (re)                                                                          \
        {                                                                                   \
            printf("Error: %s %d\n %s:%d\n", vk_result_string(re), re, __FILE__, __LINE__); \
            abort();                                                                        \
        }                                                                                   \
    }

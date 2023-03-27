// Copyright MediaZ AS. All Rights Reserved.


#include "mzVulkan/NativeAPIDirectx.h"
#include "mzVulkan/Image.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Command.h"

#include <handleapi.h>

namespace mz::vk
{

bool PlatformCloseHandle(HANDLE handle)
{
    return CloseHandle(handle);
}

HANDLE PlatformDupeHandle(u64 pid, HANDLE handle)
{
    DWORD flags;
    HANDLE re = 0;

    HANDLE src = OpenProcess(GENERIC_ALL, false, pid);
    HANDLE cur = GetCurrentProcess();

	if (!DuplicateHandle(src, handle, cur, &re, GENERIC_ALL, 0, DUPLICATE_SAME_ACCESS))                                                                                                                                                                                                    
        return 0;
	

    WIN32_ASSERT(GetHandleInformation(src, &flags));
    WIN32_ASSERT(GetHandleInformation(cur, &flags));
    WIN32_ASSERT(GetHandleInformation(re, &flags));

    WIN32_ASSERT(CloseHandle(src));

    return re;
}

u64 PlatformGetCurrentProcessId()
{
    return GetCurrentProcessId();
}

std::string GetLastErrorAsString()
{
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
    {
        return std::string(); // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,
                                 errorMessageID,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 (LPSTR)&messageBuffer,
                                 0,
                                 NULL);

    // Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

VkExternalMemoryProperties GetExportProperties(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type)
{
    VkPhysicalDeviceExternalImageFormatInfo externalimageFormatInfo = {
        .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = Type,
    };

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        .sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext  = &externalimageFormatInfo,
        .format = Format,
        .type   = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage  = Usage,
        .flags  = VK_IMAGE_CREATE_ALIAS_BIT,
    };

    VkExternalImageFormatProperties extProps = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };

    VkImageFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &extProps,
    };

    MZVK_ASSERT(vkGetPhysicalDeviceImageFormatProperties2(PhysicalDevice, &imageFormatInfo, &props));
    return extProps.externalMemoryProperties;
}

bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type)
{
    VkExternalMemoryProperties extProps = GetExportProperties(PhysicalDevice, Format, Usage, Type);

    //assert(!(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT));

    return extProps.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
}

void ImageLayoutTransition(VkImage Image,
                           rc<CommandBuffer> Cmd,
                           ImageState Src,
                           ImageState Dst, 
                           VkImageAspectFlags Aspect)
{  
    
    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = Src.Layout,
        .newLayout           = Dst.Layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
        .dstQueueFamilyIndex = Cmd->Pool->Queue->Family,
        // .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        // .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = Image,
        .subresourceRange    = {
               .aspectMask   = Aspect,
               .baseMipLevel = 0,
               .levelCount   = 1,
               .layerCount   = 1,
        },
    };

    // Put barrier inside setup command buffer
    Cmd->PipelineBarrier(Src.StageMask, Dst.StageMask, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    // Cmd->PipelineBarrier(Src.StageMask, Dst.StageMask, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, 0, 0, 0, 1, &imageMemoryBarrier);
}

void ImageLayoutTransition2(VkImage Image,
    rc<CommandBuffer> Cmd,
    ImageState Src,
    ImageState Dst, 
    VkImageAspectFlags Aspect)
{
    // Create an image barrier object
    VkImageMemoryBarrier2 imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = Src.StageMask,
        .srcAccessMask = Src.AccessMask,
        .dstStageMask = Dst.StageMask,
        .dstAccessMask = Dst.AccessMask,
        .oldLayout = Src.Layout,
        .newLayout = Dst.Layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
        .dstQueueFamilyIndex = Cmd->Pool->Queue->Family,
        // .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        // .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = Image,
        .subresourceRange = {
               .aspectMask = Aspect,
               .baseMipLevel = 0,
               .levelCount = 1,
               .layerCount = 1,
        },
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_DEVICE_GROUP_BIT,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageMemoryBarrier,
    };

    // Put barrier inside setup command buffer
    Cmd->PipelineBarrier2(&dependencyInfo);
    // Cmd->PipelineBarrier(Src.StageMask, Dst.StageMask, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, 0, 0, 0, 1, &imageMemoryBarrier);
}

std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps)
{
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);

    u32 typeIndex = 0;

    if (0 == requestedProps)
    {
        typeIndex = log2(memoryTypeBits - (memoryTypeBits & (memoryTypeBits - 1)));
    }
    else
    {
        std::vector<std::pair<u32, u32>> memoryTypes;

        for (int i = 0; i < props.memoryTypeCount; i++)
        {
            if (memoryTypeBits & (1 << i))
            {
                memoryTypes.push_back(std::make_pair(i, std::popcount(props.memoryTypes[i].propertyFlags & requestedProps)));
            }
        }

        std::sort(memoryTypes.begin(), memoryTypes.end(), [](const std::pair<u32, u32>& a, const std::pair<u32, u32>& b) { return a.second > b.second; });

        typeIndex = memoryTypes.front().first;
    }

    return std::make_pair(typeIndex, props.memoryTypes[typeIndex].propertyFlags);
}

bool IsYCbCr(VkFormat fmt)
{
    switch (fmt)
    {
    default: return false;
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
         return true;
    }
}

const char* vk_result_string(VkResult re)
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

const char* descriptor_type_to_string(VkDescriptorType ty)
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
} // namespace mz::vk
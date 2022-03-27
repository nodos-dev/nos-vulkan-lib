
#include <mzVkCommon.h>

#include <NativeAPID3D12.h>

#include <Image.h>

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

    WIN32_ASSERT(GetHandleInformation(src, &flags));
    WIN32_ASSERT(GetHandleInformation(cur, &flags));
    WIN32_ASSERT(DuplicateHandle(src, handle, cur, &re, GENERIC_ALL, 0, DUPLICATE_SAME_ACCESS));
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

bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage)
{
    VkPhysicalDeviceExternalImageFormatInfo externalimageFormatInfo = {
        .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
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

    MZ_VULKAN_ASSERT_SUCCESS(vkGetPhysicalDeviceImageFormatProperties2(PhysicalDevice, &imageFormatInfo, &props));

    assert(!(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT));

    return extProps.externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
}

void ImageLayoutTransition(VkImage Image,
                           rc<CommandBuffer> Cmd,
                           VkImageLayout CurrentLayout,
                           VkImageLayout TargetLayout,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           u32 srcQueueFamilyIndex,
                           u32 dstQueueFamilyIndex)
{
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccessMask,
        .dstAccessMask       = dstAccessMask,
        .oldLayout           = CurrentLayout,
        .newLayout           = TargetLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
        .dstQueueFamilyIndex = 0,
        .image               = Image,
        .subresourceRange    = {
               .aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel = 0,
               .levelCount   = 1,
               .layerCount   = 1,
        },
    };

    {

        // // https://github.com/SaschaWillems/Vulkan/blob/821a0659a76131662b1fc4a77c5a1ee6a9a330d8/base/Tools.cpp#L142

        // // Source layouts (old)
        // // Source access mask controls actions that have to be finished on the old layout
        // // before it will be transitioned to the new layout
        // switch (CurrentLayout)
        // {
        // case VK_IMAGE_LAYOUT_UNDEFINED:
        //     // Image layout is undefined (or does not matter)
        //     // Only valid as initial layout
        //     // No flags required, listed only for completeness
        //     imageMemoryBarrier.srcAccessMask = 0;
        //     break;

        // case VK_IMAGE_LAYOUT_PREINITIALIZED:
        //     // Image is preinitialized
        //     // Only valid as initial layout for linear images, preserves memory contents
        //     // Make sure host writes have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        //     // Image is a color attachment
        //     // Make sure any writes to the color buffer have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        //     // Image is a depth/stencil attachment
        //     // Make sure any writes to the depth/stencil buffer have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        //     // Image is a transfer source
        //     // Make sure any reads from the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        //     // Image is a transfer destination
        //     // Make sure any writes to the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        //     // Image is read by a shader
        //     // Make sure any shader reads from the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //     break;
        // default:
        //     // Other source layouts aren't handled (yet)
        //     break;
        // }

        // // Target layouts (new)
        // // Destination access mask controls the dependency for the new image layout
        // switch (TargetLayout)
        // {
        // case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        //     // Image will be used as a transfer destination
        //     // Make sure any writes to the image have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        //     // Image will be used as a transfer source
        //     // Make sure any reads from the image have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        //     // Image will be used as a color attachment
        //     // Make sure any writes to the color buffer have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        //     // Image layout will be used as a depth/stencil attachment
        //     // Make sure any writes to depth/stencil buffer have been finished
        //     imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        //     // Image will be read in a shader (sampler, input attachment)
        //     // Make sure any writes to the image have been finished
        //     if (imageMemoryBarrier.srcAccessMask == 0)
        //     {
        //         imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        //     }
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //     break;
        // default:
        //     // Other source layouts aren't handled (yet)
        //     break;
        // }
    }

    // Put barrier inside setup command buffer
    Cmd->PipelineBarrier(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        0,
        0,
        0,
        1,
        &imageMemoryBarrier);
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